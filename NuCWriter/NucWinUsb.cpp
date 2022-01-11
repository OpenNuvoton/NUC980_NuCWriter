#include <windows.h>
#include <stdio.h>
#include <fcntl.h>
#include <io.h>
#include <iostream>
#include <fstream>
#include <exception>
#include <setupapi.h>
#include <Strsafe.h>

#include "NuCWriter.h"
#include "NucWinUsb.h"

// Linked libraries
#pragma comment (lib, "Setupapi.lib")
#pragma comment (lib, "winusb.lib" )

static const GUID OSR_DEVICE_INTERFACE[] =
{ 0xD696BFEB, 0x1734, 0x417d, { 0x8A, 0x04, 0x86,0xD0,0x10,0x71,0xC5,0x12 } };


static BOOL GetDeviceHandle(void)
{
	BOOL  bResult = TRUE;
	HDEVINFO  hDeviceInfo;
	SP_DEVINFO_DATA  DeviceInfoData;
	SP_DEVICE_INTERFACE_DATA  deviceInterfaceData;
	PSP_DEVICE_INTERFACE_DETAIL_DATA  pInterfaceDetailData = NULL;
	ULONG  requiredLength=0;
	LPTSTR  lpDevicePath = NULL;
	DWORD  index = 0;
	
	if (OSR_DEVICE_INTERFACE[0] == GUID_NULL) {
		return FALSE;
	}
	
	// Get information about all the installed devices for the specified
	// device interface class.
	hDeviceInfo = SetupDiGetClassDevs(
					  &OSR_DEVICE_INTERFACE[0],
					  NULL,
					  NULL,
					  DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
	
	if (hDeviceInfo == INVALID_HANDLE_VALUE) {
		fprintf(stderr, "Error SetupDiGetClassDevs: %d.\n", GetLastError());
		goto done;
	}
	
	//Enumerate all the device interfaces in the device information set.
	DeviceInfoData.cbSize = sizeof(SP_DEVINFO_DATA);
	
	for (index = 0; SetupDiEnumDeviceInfo(hDeviceInfo, index, &DeviceInfoData); index++) {
		
		deviceInterfaceData.cbSize = sizeof(SP_INTERFACE_DEVICE_DATA);
	
		//Get information about the device interface.
		bResult = SetupDiEnumDeviceInterfaces(
					  hDeviceInfo,
					  &DeviceInfoData,
					  &OSR_DEVICE_INTERFACE[0],
					  0,
					  &deviceInterfaceData);
		
		// Check if last item
		if (GetLastError () == ERROR_NO_MORE_ITEMS) {
			goto done;
		}
		
		//Check for some other error
		if (!bResult) {
			fprintf(stderr, "Error SetupDiEnumDeviceInterfaces:0x%x.\n", GetLastError());
			goto done;
		}
		
		//Reset for this iteration
		if (lpDevicePath) {
			LocalFree(lpDevicePath);
		}
		if (pInterfaceDetailData) {
			LocalFree(pInterfaceDetailData);
		}
		
		//Interface data is returned in SP_DEVICE_INTERFACE_DETAIL_DATA
		//which we need to allocate, so we have to call this function twice.
		//First to get the size so that we know how much to allocate
		//Second, the actual call with the allocated buffer
		
		bResult = SetupDiGetDeviceInterfaceDetail(
					  hDeviceInfo,
					  &deviceInterfaceData,
					  NULL, 0,
					  &requiredLength,
					  NULL);
		
		
		//Check for some other error
		if (!bResult) {
			if ((ERROR_INSUFFICIENT_BUFFER == GetLastError()) && (requiredLength>0)) {
				//we got the size, allocate buffer
				pInterfaceDetailData = (PSP_DEVICE_INTERFACE_DETAIL_DATA)LocalAlloc(LPTR, requiredLength);
				if (!pInterfaceDetailData) {
					fprintf(stderr, "Error allocating memory for the device detail buffer.\n");
					goto done;
				}
			} else {
				fprintf(stderr, "Error SetupDiEnumDeviceInterfaces: 0x%x.\n", GetLastError());
				goto done;
			}
		}
		
		//get the interface detailed data
		pInterfaceDetailData->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);
		
		//Now call it with the correct size and allocated buffer
		bResult = SetupDiGetDeviceInterfaceDetail(
					  hDeviceInfo,
					  &deviceInterfaceData,
					  pInterfaceDetailData,
					  requiredLength,
					  NULL,
					  &DeviceInfoData);
		
		//Check for some other error
		if (!bResult) {
			fprintf(stderr, "Error SetupDiGetDeviceInterfaceDetail - index: %d, err: 0x%x.\n", index, GetLastError());
			goto done;
		}
		
		//copy device path
		
		size_t nLength = lstrlen(pInterfaceDetailData->DevicePath) + 1;
		lpDevicePath = (TCHAR *) LocalAlloc (LPTR, nLength * sizeof(TCHAR));
		StringCchCopy(lpDevicePath, nLength, pInterfaceDetailData->DevicePath);
		// strncpy_s(lpDevicePath, nLength, pInterfaceDetailData->DevicePath, nLength); 
		lpDevicePath[nLength-1] = 0;
		
		// Open the device
		_WinUsb.WinUsbHandle[index].hDeviceHandle = CreateFile (
												lpDevicePath,
												GENERIC_READ | GENERIC_WRITE,
												FILE_SHARE_READ | FILE_SHARE_WRITE,
												NULL,
												OPEN_EXISTING,
												FILE_FLAG_OVERLAPPED,
												NULL);
		
		if (_WinUsb.WinUsbHandle[index].hDeviceHandle == INVALID_HANDLE_VALUE) {
			fprintf(stderr, "Open device %d Error 0x%x.", index, GetLastError());
			bResult = FALSE;
			goto done;
		}
		//_WinUsb.WinUsbHandle[index].DevicePath.Format("%s", lpDevicePath);
		strcpy_s(_WinUsb.WinUsbHandle[index].DevicePath, 
				sizeof(_WinUsb.WinUsbHandle[index].DevicePath), (LPCTSTR)lpDevicePath);
		_WinUsb.WinUsbNumber = index + 1;
		_WinUsb.WinUsbHandle[index].HandlesOpen = TRUE;
	}
	
done:
	LocalFree(lpDevicePath);
	LocalFree(pInterfaceDetailData);
	bResult = SetupDiDestroyDeviceInfoList(hDeviceInfo);
	return bResult;
}

static BOOL GetWinUSBHandle(void)
{
	int  i = 0;
	BOOL  bResult = FALSE;
	
	for (i = 0; i < _WinUsb.WinUsbNumber ; i++) {
		if (_WinUsb.WinUsbHandle[i].hDeviceHandle == INVALID_HANDLE_VALUE)
			return FALSE;
		bResult = WinUsb_Initialize(_WinUsb.WinUsbHandle[i].hDeviceHandle,
						&(_WinUsb.WinUsbHandle[i].hUSBHandle));
		if (!bResult) {
			fprintf(stderr, "GetWinUSBHandle failed on device %d.\n", i);
			return FALSE;
		}
	}
	return bResult;
}

static BOOL GetUSBDeviceSpeed()
{
	int	i = 0;
	BOOL	bResult = TRUE;
	ULONG	length = sizeof(UCHAR);
	UCHAR	DeviceSpeed;
	
	for (i = 0; i < _WinUsb.WinUsbNumber ; i++) {
		if (_WinUsb.WinUsbHandle[i].hDeviceHandle == INVALID_HANDLE_VALUE)
			return FALSE;
	
		bResult = WinUsb_QueryDeviceInformation(_WinUsb.WinUsbHandle[i].hUSBHandle, DEVICE_SPEED, &length, &DeviceSpeed);
		if (!bResult) {
			fprintf(stderr, "%s %d - Error getting device speed: 0x%x.\n", __func__, __LINE__, GetLastError());
			return bResult;
		}
	}
	return bResult;
	//if(*pDeviceSpeed == LowSpeed)
	//if(*pDeviceSpeed == FullSpeed)
	//if(*pDeviceSpeed == HighSpeed)
}

static BOOL QueryDeviceEndpoints(void)
{
	int	i = 0;
	PIPE_ID	*pipeid;
	BOOL	bResult = TRUE;
	WINUSB_INTERFACE_HANDLE hUSBHandle;
	//ULONG timeout = 20000;
	ULONG timeout = 10000;
	
	for (i = 0; i < _WinUsb.WinUsbNumber ; i++) {
		hUSBHandle = _WinUsb.WinUsbHandle[i].hUSBHandle;
		pipeid = &(_WinUsb.WinUsbHandle[i].pipeid);
		if (hUSBHandle == INVALID_HANDLE_VALUE)
			return FALSE;
		
		USB_INTERFACE_DESCRIPTOR InterfaceDescriptor;
		ZeroMemory(&InterfaceDescriptor, sizeof(USB_INTERFACE_DESCRIPTOR));
		
		WINUSB_PIPE_INFORMATION  Pipe;
		ZeroMemory(&Pipe, sizeof(WINUSB_PIPE_INFORMATION));
		
		bResult = WinUsb_QueryInterfaceSettings(hUSBHandle, 0, &InterfaceDescriptor);
		
		if (bResult) {
			for (int index = 0; index < InterfaceDescriptor.bNumEndpoints; index++) {
				bResult = WinUsb_QueryPipe(hUSBHandle, 0, index, &Pipe);
				
				if (bResult) {
					if (Pipe.PipeType == UsbdPipeTypeControl) {
						// fprintf(stdout, "Endpoint index: %d Pipe type: Control<0x%x> Pipe ID: 0x%x.\n", index, Pipe.PipeType, Pipe.PipeId);
					}
					if (Pipe.PipeType == UsbdPipeTypeIsochronous) {
						// fprintf(stdout, "Endpoint index: %d Pipe type: Isochronous<0x%x> Pipe ID: 0x%x.\n", index, Pipe.PipeType, Pipe.PipeId);
					}
					if (Pipe.PipeType == UsbdPipeTypeBulk) {
						if (USB_ENDPOINT_DIRECTION_IN(Pipe.PipeId)) {
							// fprintf(stdout, "Endpoint index: %d Pipe type: Bulk<0x%x> Pipe ID: 0x%x.\n", index, Pipe.PipeType, Pipe.PipeId);
							pipeid->PipeInId = Pipe.PipeId;
							WinUsb_SetPipePolicy(hUSBHandle, Pipe.PipeId, PIPE_TRANSFER_TIMEOUT, sizeof(timeout), &timeout);
						}
						if (USB_ENDPOINT_DIRECTION_OUT(Pipe.PipeId)) {
							// fprintf(stdout, "Endpoint index: %d Pipe type: Bulk<0x%x> Pipe ID: 0x%x.\n", index, Pipe.PipeType, Pipe.PipeId);
							pipeid->PipeOutId = Pipe.PipeId;
							WinUsb_SetPipePolicy(hUSBHandle, Pipe.PipeId, PIPE_TRANSFER_TIMEOUT, sizeof(timeout), &timeout);							
						}
					}
					if (Pipe.PipeType == UsbdPipeTypeInterrupt) {
						fprintf(stdout, "Endpoint index: %d Pipe type: Interrupt<0x%x> Pipe ID: 0x%x.\n", index, Pipe.PipeType, Pipe.PipeId);
					}
				} else {
					continue;
				}
			}
		}
		
	}
//done:
	return bResult;
}

BOOL EnableWinUsbDevice()
{
	int	i;
	BOOL	bResult;

	bResult = GetDeviceHandle();
	if (!bResult) {
		fprintf(stdout, "XXX GetDeviceHandle error\n");
		return bResult;
	}
	
	bResult = GetWinUSBHandle();
	if (!bResult) {
		for (i = 0; i < _WinUsb.WinUsbNumber; i++)
			CloseHandle(_WinUsb.WinUsbHandle[i].hDeviceHandle);
		_WinUsb.WinUsbNumber = 0;
		fprintf(stderr, "GetWinUSBHandle error\n");
		return bResult;
	}
	
	bResult = GetUSBDeviceSpeed();
	if (!bResult) {
		fprintf(stderr, "XXX GetUSBDeviceSpeed error\n");
		return bResult;
	}
	
	bResult = QueryDeviceEndpoints();
	if (!bResult) {
		for (i = 0; i < _WinUsb.WinUsbNumber; i++) {
			CloseHandle(_WinUsb.WinUsbHandle[i].hDeviceHandle);
			WinUsb_Free(_WinUsb.WinUsbHandle[i].hUSBHandle);
		}
		_WinUsb.WinUsbNumber = 0;
		fprintf(stderr, "XXX QueryDeviceEndpoints error\n");
		return bResult;
	}
	Sleep(100);
	return bResult;
}

static BOOL OpenDevice(int id)
{
	BOOL	bResult = FALSE;
	HDEVINFO hDeviceInfo;
	SP_DEVINFO_DATA DeviceInfoData;
	SP_DEVICE_INTERFACE_DATA deviceInterfaceData;
	PSP_DEVICE_INTERFACE_DETAIL_DATA pInterfaceDetailData = NULL;
	ULONG	requiredLength = 0;
	LPTSTR	lpDevicePath = NULL;
	size_t	nLength;
	
	if (OSR_DEVICE_INTERFACE[0] == GUID_NULL) {
		return FALSE;
	}
	_WinUsb.WinUsbHandle[id].HandlesOpen = FALSE;
	// Get information about all the installed devices for the specified
	// device interface class.
	hDeviceInfo = SetupDiGetClassDevs(
					&OSR_DEVICE_INTERFACE[0],
					NULL,
					NULL,
					DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
	
	if (hDeviceInfo == INVALID_HANDLE_VALUE) {
		// ERROR
		fprintf(stderr, "%s %d - Error SetupDiGetClassDevs: 0x%x.\n", __func__, __LINE__, GetLastError());
		goto done;
	}
	//Enumerate all the device interfaces in the device information set.
	DeviceInfoData.cbSize = sizeof(SP_DEVINFO_DATA);
	
	bResult = SetupDiEnumDeviceInfo(hDeviceInfo, id, &DeviceInfoData);
	if (!bResult && (GetLastError() != ERROR_NO_MORE_ITEMS)) {
		fprintf(stderr, "%s %d - Error OpenDevice SetupDiEnumDeviceInfo: 0x%x.\n", __func__, __LINE__, GetLastError());
		goto done;
	}
	deviceInterfaceData.cbSize = sizeof(SP_INTERFACE_DEVICE_DATA);
	//Get information about the device interface.
	bResult = SetupDiEnumDeviceInterfaces(
					hDeviceInfo,
					&DeviceInfoData,
					&OSR_DEVICE_INTERFACE[0],
					0,
					&deviceInterfaceData);
	// Check if last item
	if (GetLastError () == ERROR_NO_MORE_ITEMS) {
		goto done;
	}
	//Check for some other error
	if (!bResult) {
		fprintf(stdout, "%s %d - Error SetupDiEnumDeviceInterfaces: 0x%x.\n", __func__, __LINE__, GetLastError());
		goto done;
	}
	
	//Reset for this iteration
	if (lpDevicePath) {
		LocalFree(lpDevicePath);
	}
	if (pInterfaceDetailData) {
		LocalFree(pInterfaceDetailData);
	}
	
	//Interface data is returned in SP_DEVICE_INTERFACE_DETAIL_DATA
	//which we need to allocate, so we have to call this function twice.
	//First to get the size so that we know how much to allocate
	//Second, the actual call with the allocated buffer
	
	bResult = SetupDiGetDeviceInterfaceDetail(
					hDeviceInfo,
					&deviceInterfaceData,
					NULL, 0,
					&requiredLength,
					NULL);

	//Check for some other error
	if (!bResult) {
		if ((ERROR_INSUFFICIENT_BUFFER==GetLastError()) && (requiredLength>0)) {
			//we got the size, allocate buffer
			pInterfaceDetailData = (PSP_DEVICE_INTERFACE_DETAIL_DATA)LocalAlloc(LPTR, requiredLength);
			
			if (!pInterfaceDetailData) {
				// ERROR
				fprintf(stdout, "%s %d - Error allocating memory for the device detail buffer.\n", __func__, __LINE__);
				goto done;
			}
		} else {
			fprintf(stderr, "%s %d - Error SetupDiEnumDeviceInterfaces: 0x%x.\n", __func__, __LINE__, GetLastError());
			goto done;
		}
	}
	
	//get the interface detailed data
	pInterfaceDetailData->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);
	
	//Now call it with the correct size and allocated buffer
	bResult = SetupDiGetDeviceInterfaceDetail(
					hDeviceInfo,
					&deviceInterfaceData,
					pInterfaceDetailData,
					requiredLength,
					NULL,
					&DeviceInfoData);
	
	//Check for some other error
	if (!bResult) {
		fprintf(stderr, "%s %d - Error SetupDiGetDeviceInterfaceDetail:0x%x.\n", __func__, __LINE__, GetLastError());
		goto done;
	}
	
	//copy device path
	
	nLength = lstrlen(pInterfaceDetailData->DevicePath) + 1;
	lpDevicePath = (TCHAR *) LocalAlloc (LPTR, nLength * sizeof(TCHAR));
	StringCchCopy(lpDevicePath, nLength, pInterfaceDetailData->DevicePath);
	lpDevicePath[nLength-1] = 0;
	
	//fprintf(stdout, "Device path:  %s\n", lpDevicePath);
	if (!lpDevicePath) {
		fprintf(stderr, "%s %d - Error 0x%x.", __func__, __LINE__, GetLastError());
		goto done;
	}
	
	//Open the device
	_WinUsb.WinUsbHandle[id].hDeviceHandle = CreateFile (
										 lpDevicePath,
										 GENERIC_READ | GENERIC_WRITE,
										 FILE_SHARE_READ | FILE_SHARE_WRITE,
										 NULL,
										 OPEN_EXISTING,
										 FILE_FLAG_OVERLAPPED,
										 NULL);
	
	if (_WinUsb.WinUsbHandle[id].hDeviceHandle == INVALID_HANDLE_VALUE) {
		fprintf(stdout, "%s %d - CreateFile Error 0x%x.", __func__, __LINE__, GetLastError());
		goto done;
	}
	
	//_WinUsb.WinUsbHandle[id].DevicePath.Format("%s", lpDevicePath);
	strcpy_s(_WinUsb.WinUsbHandle[id].DevicePath, MAX_PATH, lpDevicePath);
	
	bResult = WinUsb_Initialize(_WinUsb.WinUsbHandle[id].hDeviceHandle,
								&(_WinUsb.WinUsbHandle[id].hUSBHandle));
	if (FALSE == bResult) {
		CloseHandle(_WinUsb.WinUsbHandle[id].hDeviceHandle);
		fprintf(stderr, "%s %d - WinUsb_Initialize Error\n", __func__, __LINE__);
		goto done;
	}
	_WinUsb.WinUsbHandle[id].HandlesOpen = TRUE;
	
	// fprintf(stdout, "%s %d (%d) - USB device opened.\n", __func__, __LINE__, id);

done:
	LocalFree(lpDevicePath);
	LocalFree(pInterfaceDetailData);
	SetupDiDestroyDeviceInfoList(hDeviceInfo);
	return bResult;
}

BOOL CloseWinUsbDevice(int id)
{
	BOOL bResult = TRUE;
	
	// fprintf(stdout, "CloseWinUsbDevice id=%d\n",id);
	if (FALSE == _WinUsb.WinUsbHandle[id].HandlesOpen) {
		// Called on an uninitialized DeviceData
		fprintf(stderr, "%s %d - WinUsbHandle[%d] is not Open.\n", __func__, __LINE__, id);
		return FALSE;
	}
	CloseHandle(_WinUsb.WinUsbHandle[id].hDeviceHandle);
	WinUsb_Free(_WinUsb.WinUsbHandle[id].hUSBHandle);
	_WinUsb.WinUsbHandle[id].hDeviceHandle = INVALID_HANDLE_VALUE;
	_WinUsb.WinUsbHandle[id].hUSBHandle = INVALID_HANDLE_VALUE;
	_WinUsb.WinUsbHandle[id].HandlesOpen = FALSE;

	//Sleep(150);
	return TRUE;
}

static BOOL GetOneUSBDeviceSpeed(int id)
{
	BOOL	bResult = TRUE;
	ULONG	length = sizeof(UCHAR);
	UCHAR	DeviceSpeed;
	
	if (_WinUsb.WinUsbHandle[id].hDeviceHandle == INVALID_HANDLE_VALUE)
		return FALSE;
	
	bResult = WinUsb_QueryDeviceInformation(_WinUsb.WinUsbHandle[id].hUSBHandle, DEVICE_SPEED, &length, &DeviceSpeed);
	if(!bResult) {
		fprintf(stderr, "XXX Error getting device speed: 0x%x.\n", GetLastError());
		return bResult;
	}
	if (DeviceSpeed == LowSpeed) {
		fprintf(stderr, "Device speed: %d (Low speed).\n", DeviceSpeed);
		goto done;
	}
	if (DeviceSpeed == FullSpeed) {
		fprintf(stderr, "Device speed: %d (Full speed).\n", DeviceSpeed);
		goto done;
	}
	if (DeviceSpeed == HighSpeed) {
		// fprintf(stdout, "Device speed: %d (High speed).\n", DeviceSpeed);
		goto done;
	}

done:
	return bResult;
}

static BOOL QueryOneDeviceEndpoints(int id)
{
	PIPE_ID	*pipeid;
	BOOL	bResult = TRUE, nStatus;
	WINUSB_INTERFACE_HANDLE hUSBHandle;
	USB_INTERFACE_DESCRIPTOR InterfaceDescriptor;
	WINUSB_PIPE_INFORMATION  Pipe;
	// Set the transfer time-out interval for endpoint 0 to 10000 milliseconds.
	//ULONG timeout = 20000;
	ULONG	timeout = 10000;
	
	hUSBHandle = _WinUsb.WinUsbHandle[id].hUSBHandle;
	pipeid = &(_WinUsb.WinUsbHandle[id].pipeid);
	if (hUSBHandle == INVALID_HANDLE_VALUE)
		return FALSE;
		
	ZeroMemory(&InterfaceDescriptor, sizeof(USB_INTERFACE_DESCRIPTOR));
	ZeroMemory(&Pipe, sizeof(WINUSB_PIPE_INFORMATION));
	
	bResult = WinUsb_QueryInterfaceSettings(hUSBHandle, 0, &InterfaceDescriptor);
	
	if (bResult) {
		for (int index = 0; index < InterfaceDescriptor.bNumEndpoints; index++) {
			bResult = WinUsb_QueryPipe(hUSBHandle, 0, index, &Pipe);
			
			if (bResult) {
				if (Pipe.PipeType == UsbdPipeTypeControl) {
					//fprintf(stdout, "Endpoint index: %d Pipe type: Control<0x%x> Pipe ID: 0x%x.\n", index, Pipe.PipeType, Pipe.PipeId);
				}
				if (Pipe.PipeType == UsbdPipeTypeIsochronous) {
					//fprintf(stdout, "Endpoint index: %d Pipe type: Isochronous<0x%x> Pipe ID: 0x%x.\n", index, Pipe.PipeType, Pipe.PipeId);
				}
				if (Pipe.PipeType == UsbdPipeTypeBulk) {
					if (USB_ENDPOINT_DIRECTION_IN(Pipe.PipeId)) {
						//fprintf(stdout, "Endpoint index: %d Pipe type: Bulk IN<0x%x> Pipe ID: 0x%x.\n", index, Pipe.PipeType, Pipe.PipeId);
						pipeid->PipeInId = Pipe.PipeId;
						nStatus = WinUsb_SetPipePolicy(hUSBHandle,
														Pipe.PipeId, PIPE_TRANSFER_TIMEOUT,
														sizeof(timeout), &timeout);
						if (nStatus == FALSE) {
							fprintf(stdout, "XXX Bulk IN  errorType = 0x%x\n", GetLastError());
							return FALSE;
						}
					}
					if (USB_ENDPOINT_DIRECTION_OUT(Pipe.PipeId)) {
						//fprintf(stdout, "Endpoint index: %d Pipe type: Bulk OUT<0x%x> Pipe ID: 0x%x.\n", index, Pipe.PipeType, Pipe.PipeId);
						pipeid->PipeOutId = Pipe.PipeId;
						nStatus = WinUsb_SetPipePolicy(hUSBHandle,
														Pipe.PipeId, PIPE_TRANSFER_TIMEOUT,
														sizeof(timeout), &timeout);
						if (nStatus == FALSE) {
							fprintf(stdout, "XXX Bulk OUT  errorType = 0x%x\n", GetLastError());
							return FALSE;
						}
					}
				}
				if (Pipe.PipeType == UsbdPipeTypeInterrupt) {
					//fprintf(stdout, "Endpoint index: %d Pipe type: Interrupt<0x%x> Pipe ID: 0x%x.\n", index, Pipe.PipeType, Pipe.PipeId);
				}
			} else {
				continue;
			}
		}
	}
	//done:
	return bResult;
}

BOOL EnableOneWinUsbDevice(int id)
{
	BOOL bResult = TRUE;
	
	// fprintf(stdout, "#### EnableOneWinUsbDevice id=%d\n",id);
	
	if (_WinUsb.WinUsbHandle[id].HandlesOpen == TRUE) {
#if 1
		return TRUE;
#else
		fprintf(stderr, "%s - Device %d has been enabled by other thread\n", __func__, id);
		//CloseHandle(_WinUsb.WinUsbHandle[id].hDeviceHandle);
		//WinUsb_Free(_WinUsb.WinUsbHandle[id].hUSBHandle);
		CloseHandle(WinUsbHandle[id].hDeviceHandle);
		WinUsb_Free(WinUsbHandle[id].hUSBHandle);
		WinUsbHandle[id].hDeviceHandle=INVALID_HANDLE_VALUE;
		WinUsbHandle[id].hUSBHandle=INVALID_HANDLE_VALUE;
		WinUsbHandle[id].HandlesOpen = FALSE;
		// return FALSE;
#endif
	}

	bResult = OpenDevice(id);
	if (!bResult) {
		fprintf(stderr, "%s - (%d)  OpenDevice: failed, try again\n", __func__, id);
		Sleep(500);
		bResult = OpenDevice(id);
		if (!bResult) {
			fprintf(stderr, "#### (%d) XXX OpenDevice Error\n", id);
			return bResult;
		}
	}
	
	Sleep(200);  //for Mass production
	bResult = GetOneUSBDeviceSpeed(id);
	if (!bResult) {
		fprintf(stderr, "#### (%d) XXX GetOneUSBDeviceSpeed\n", id);
		CloseWinUsbDevice(id);
		return bResult;
	}
	
	Sleep(200);  //for Mass production
	bResult = QueryOneDeviceEndpoints(id);
	if (!bResult) {
		fprintf(stderr, "#### (%d) XXX QueryOneDeviceEndpoints\n", id);
		CloseWinUsbDevice(id);
		return bResult;
	}
	Sleep(200);
	return bResult;
}

BOOL WinUsb_ResetFW(int id)
{
	BOOL	bResult = FALSE;
	ULONG	nBytesRead = 0;
	WINUSB_SETUP_PACKET SetupPacket;
	
	bResult = EnableOneWinUsbDevice(id);
	
	if (bResult) {
		ZeroMemory(&SetupPacket, sizeof(WINUSB_SETUP_PACKET));
		SetupPacket.RequestType = 0x46;//WM_RESET;
		SetupPacket.Request = 0;
		SetupPacket.Value = 0;
		SetupPacket.Index = 0;
		SetupPacket.Length = 0;
		
		WinUsb_ControlTransfer(_WinUsb.WinUsbHandle[id].hUSBHandle, SetupPacket, 0, 0, &nBytesRead, 0);
		CloseWinUsbDevice(id);
	} else {
		CloseWinUsbDevice(id);
		return FALSE;
	}
	
	return TRUE;
}

BOOL NUC_ReadPipe(int id, UCHAR *buf, ULONG len)
{
	ULONG	nBytesRead = 0;
	volatile int  i;
	BOOL	bResult;
	
	//fprintf(stdout, "%s %d -  (%d)\n", __func__, __LINE__, id);
	
	if ((id >= _WinUsb.WinUsbNumber) || (id < 0)) {
		fprintf(stderr, "%s - id (%d) error! (%d)\n", __func__, id, _WinUsb.WinUsbNumber);
		return FALSE;
	}
	
	if (FALSE == _WinUsb.WinUsbHandle[id].HandlesOpen ) {
		fprintf(stderr, "xxxxx %s (%d) USB device is not Open.\n", __func__, id);
		return FALSE;
	}

	// for (i = 0x0; i < 0x4000; i++);
	for (i = 0x0; i < 0x2000; i++);
	bResult = WinUsb_ReadPipe(_WinUsb.WinUsbHandle[id].hUSBHandle,
								_WinUsb.WinUsbHandle[id].pipeid.PipeInId,
								(unsigned char*)buf, len, &nBytesRead, 0);
	if (bResult == FALSE) {
		fprintf(stderr, "XXXX IN(%d) pipeId=0x%x   len=%d    nBytesRead=%d\n",
							id, _WinUsb.WinUsbHandle[id].pipeid.PipeOutId, len, nBytesRead);
		fprintf(stderr, "Error WinUsb_ReadPipe: 0x%x.\n", GetLastError());
		return FALSE;
	}
	return TRUE;
}

BOOL NUC_WritePipe(int id, UCHAR *buf, ULONG len)
{
	ULONG	nBytesSent = 0;
	WINUSB_SETUP_PACKET SetupPacket;
	BOOL	bResult;
	
	if (FALSE == _WinUsb.WinUsbHandle[id].HandlesOpen ) {
		fprintf(stderr, "XXXXX %s %d (%d) USB device is not Open.\n", __func__, __LINE__, id);
		return FALSE;
	}

	/* 
	 * Send vendor command first
	 */
	ZeroMemory(&SetupPacket, sizeof(WINUSB_SETUP_PACKET));
	//Create the setup packet
	SetupPacket.RequestType = 0x40;
	SetupPacket.Request = 0xA0;
	SetupPacket.Value = 0x12;
	SetupPacket.Index = (USHORT)len;
	SetupPacket.Length = 0;
	bResult = WinUsb_ControlTransfer(_WinUsb.WinUsbHandle[id].hUSBHandle,
									SetupPacket, 0, 0, &nBytesSent, 0);
	if (bResult == FALSE) {
		fprintf(stderr, "XXXX %s %d (%d) - CTRL hUSBHandle=0x%x,  nBytesSent=%d\n",
						__func__, __LINE__, id, _WinUsb.WinUsbHandle[id].pipeid.PipeOutId, nBytesSent);
		return FALSE;
	}
	/********************************************/
	
	nBytesSent = 0;
	bResult = WinUsb_WritePipe(_WinUsb.WinUsbHandle[id].hUSBHandle,
								_WinUsb.WinUsbHandle[id].pipeid.PipeOutId,
								buf, len, &nBytesSent, 0);
	if (bResult == FALSE) {
		fprintf(stderr, "XXXXX %s %d Error WinUsb_WritePipe: 0x%x.\n", __func__, __LINE__, GetLastError());
		return FALSE;
	}

	// fprintf(stdout, "%s %d - %d/%d bytes written.\n", __func__, __LINE__, nBytesSent, len);

	if (nBytesSent != len) {
		fprintf(stderr, "XXXXX %s %d Error nBytesSent %d/%d\n", __func__, __LINE__, nBytesSent, len);
		return FALSE;
	}
	return TRUE;
}

BOOL NUC_SetType(int id, USHORT type, UCHAR * ack, ULONG len)
{
	ULONG	nBytesRead = 0;
	BOOL	bResult;
	WINUSB_SETUP_PACKET SetupPacket;

	if ((id >= _WinUsb.WinUsbNumber) || (id < 0)) {
		fprintf(stderr, "%s - id (%d) error! (%d)\n", __func__, id, _WinUsb.WinUsbNumber);
		return FALSE;
	}

	do {
		ZeroMemory(&SetupPacket, sizeof(WINUSB_SETUP_PACKET));
		//Create the setup packet
		SetupPacket.RequestType = 0x40;
		SetupPacket.Request = BURN;
		SetupPacket.Value = BURN_TYPE+(UCHAR)type;
		SetupPacket.Index = 0;
		SetupPacket.Length = 0;
		bResult = WinUsb_ControlTransfer(_WinUsb.WinUsbHandle[id].hUSBHandle,
										SetupPacket, 0, 0, &nBytesRead, 0);
		if (bResult==FALSE) {
			fprintf(stderr, "%s (%d) - Error WinUsb_ControlTransfer: %d.\n", __func__, id, GetLastError());
			return FALSE;
		}

		ZeroMemory(&SetupPacket, sizeof(WINUSB_SETUP_PACKET));
		//Create the setup packet
		SetupPacket.RequestType = 0xC0;
		SetupPacket.Request = BURN;
		SetupPacket.Value = BURN_TYPE + (UCHAR)type;
		SetupPacket.Index = 0;
		SetupPacket.Length = (USHORT)len;
		bResult = WinUsb_ControlTransfer(_WinUsb.WinUsbHandle[id].hUSBHandle,
							SetupPacket, (unsigned char *)ack, len, &nBytesRead, 0);
		if (bResult == FALSE)
			return FALSE;

	} while((UCHAR)(*ack & 0xFF) != SetupPacket.Value);
	return TRUE;
}

