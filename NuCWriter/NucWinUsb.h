
#ifndef NUCWINUSP_H
#define NUCWINUSP_H

#include <atlstr.h>
#include "winusb.h"

#define MAX_DEVICE_NUM		32

typedef struct PIPE_ID {
    UCHAR  PipeInId;
    UCHAR  PipeOutId;
} Pipe_Id, *PPipe_Id;

typedef struct WINUSBHANDLE {
	bool	HandlesOpen; //add handlesopen for device open display
	HANDLE	hDeviceHandle;
	WINUSB_INTERFACE_HANDLE hUSBHandle;
	Pipe_Id	pipeid;
	char	DevicePath[MAX_PATH];
} SWinUsbHandle, *PSWinUsbHandle;

typedef struct _WinUSB_T {
	SWinUsbHandle WinUsbHandle[MAX_DEVICE_NUM];
	int	WinUsbNumber;
	int	m_ActiveDevice;
	UCHAR	DeviceSpeed;
} WinUSB_T;

extern WinUSB_T  _WinUsb;

extern BOOL EnableWinUsbDevice(void);
extern BOOL CloseWinUsbDevice(int id);
extern BOOL EnableOneWinUsbDevice(int id);
extern BOOL WinUsb_ResetFW(int id);
extern BOOL NUC_ReadPipe(int id, UCHAR *buf, ULONG len);
extern BOOL NUC_WritePipe(int id, UCHAR *buf, ULONG len);
extern BOOL NUC_SetType(int id, USHORT type, UCHAR * ack, ULONG len);

#endif