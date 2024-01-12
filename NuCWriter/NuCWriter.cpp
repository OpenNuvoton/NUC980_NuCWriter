// NuCWriter.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <windows.h>
#include <stdio.h>
#include <fcntl.h>
#include <io.h>
#include <iostream>
#include <fstream>
#include <sysinfoapi.h>

#include "NuCWriter.h"
#include "NucWinUsb.h"

NU_DATA_T	_nudata;
DEV_CTRL_T	_devctrl;
PACK_T		_pack;
WinUSB_T	_WinUsb;
NORBOOT_MMC_HEAD _mmc_head;

void print_usage(void);


void print_usage(void)
{
	fprintf(stdout, "==============================================\n");
	fprintf(stdout, "==   Nuvoton NuWriter Command Tool V%s   ==\n", NUCWRITER_VER);
	fprintf(stdout, "==============================================\n");
	fprintf(stdout, "NuWriter [File]\n\n");
	fprintf(stdout, "[File]      Set ini file\n");
}

void usleep(unsigned int usec)
{
	HANDLE timer;
	LARGE_INTEGER ft;

	ft.QuadPart = -(10 * (__int64)usec);

	timer = CreateWaitableTimer(NULL, TRUE, NULL);
	SetWaitableTimer(timer, &ft, 0, NULL, NULL, 0);
	WaitForSingleObject(timer, INFINITE);
	CloseHandle(timer);
}

int LoadDDRInit(char *pathName, char *ddr_sbuff, int *len)
{
	FILE		*fp;
	int			length, tmpbuf_size;
	char		*ptmpbuf, *ptmp,tmp[256], cvt[128];
	unsigned int * puint32_t;
	unsigned int val;
	int			ret = 0;
	
	if (fopen_s(&fp, pathName, "rb") != 0)
	{
		fprintf(stderr, "Error! Open DDR initial file %s error\n", pathName);
		return -1;
	}

	fseek(fp, 0, SEEK_END);
	length = ftell(fp);
	fseek(fp, 0, SEEK_SET);
	ptmpbuf = (char *)malloc(length + 32);
	// fprintf(stdout, "%s --> length = %d\n", __func__, length);
	
	puint32_t = (unsigned int *)ptmpbuf;
	tmpbuf_size = 0;
	while (!feof(fp)) {
		fgets(tmp, 256, fp);
		ptmp = strchr(tmp, '=');
		if (ptmp == NULL) {
			fprintf(stderr, "Error! DDR initial format error\n");
			ret = ERR_CODE_INI_FILE;
			goto func_out;
		}
		strncpy_s(cvt, sizeof(cvt), tmp, ptmp - tmp);
		cvt[ptmp - tmp]='\0';
		
		val = strtoul(cvt, NULL, 0);
		*puint32_t = val;
		puint32_t++;

		strncpy_s(cvt, sizeof(cvt), ++ptmp, strlen(ptmp));
		cvt[strlen(ptmp)] = '\0';
		val = strtoul(cvt, NULL, 0);
		*puint32_t = val;
		puint32_t++;
		tmpbuf_size += 8;
	}
	fclose(fp);

#if 0 //for test
	fopen_s(&fp, "./tmp_ddrini.bin", "wb");
	fwrite(ptmpbuf, 1, tmpbuf_size, fp);
	fclose(fp);
#endif

	// fprintf(stdout, "ptmpbuf[0~3] = 0x%x 0x%x 0x%x 0x%x\n", ptmpbuf[0], ptmpbuf[1], ptmpbuf[2], ptmpbuf[3]);
	if (((ptmpbuf[0] & 0xff) == 0) && ((ptmpbuf[1] & 0xff) == 0) &&
		((ptmpbuf[2] & 0xff) == 0) && ((ptmpbuf[3] & 0xff) == 0xB0))
	{
		*len = (tmpbuf_size - 8);   // ddr version 8 byte
		// DDRFileVer.Format(_T("V%d.0"), ptmpbuf[4]);
		fprintf(stdout, "DDR version: V%d.0\n", ptmpbuf[4]);
		memcpy((char *)ddr_sbuff, ptmpbuf+8, *len);
	}
	else
	{
		*len = tmpbuf_size;
		// DDRFileVer.Format(_T("No version"));
		fprintf(stdout, "DDR version: No version\n");
		memcpy((char *)ddr_sbuff, ptmpbuf, *len);
	}

func_out:
	free(ptmpbuf);
	fclose(fp);	
	return ret;
}

BOOL FWDownload(int id)
{
	int		i = 0;
	CString	ddr_fname = _nudata.ddr_file_name;
	CString nudata_path = NUDATA_PATH;
	char	filename[MAX_PATH];
	BOOL	bResult = 1;
	
	switch (ddr_fname.GetAt(8)) {
	case '5':
		sprintf_s(filename, sizeof(filename), "%s/xusb.bin", NUDATA_PATH);
		break;
	case '6':
		sprintf_s(filename, sizeof(filename), "%s/xusb64.bin", NUDATA_PATH);
		break;
	case '7':
		sprintf_s(filename, sizeof(filename), "%s/xusb128.bin", NUDATA_PATH);
		break;
	default:
		sprintf_s(filename, sizeof(filename), "%s/xusb16.bin", NUDATA_PATH);
		break;
	};
	
	bResult = XUSB(id, filename);
	
	if (!bResult) {
		CloseWinUsbDevice(id);
	}
	return bResult;
}

int InfoFromDevice(int id)
{
	int		bResult;
	unsigned int ack;
	USHORT	typeack;
	
	if (EnableOneWinUsbDevice(id) < 0) {
		fprintf(stderr, "%s (%d) - open device failed %d.\n", __func__, id, __LINE__);
		return -1;
	}
	NUC_SetType(id, INFO, (UCHAR *)&typeack, sizeof(typeack));

	bResult = NUC_WritePipe(0, (UCHAR *)&(_nudata.user_def), sizeof(INFO_T));
	if (bResult < 0) {
		fprintf(stderr, "%s (%d) - NUC_WritePipe failed %d.\n", __func__, id, __LINE__);
		goto EXIT;
	}
	
	bResult = NUC_ReadPipe(0, (UCHAR *)&(_nudata.user_def), sizeof(INFO_T));
	if (bResult < 0) {
		fprintf(stderr, "%s (%d) - NUC_ReadPipe failed %d.\n", __func__, id, __LINE__);
		goto EXIT;
	}

	bResult = NUC_ReadPipe(0, (UCHAR *)&ack, 4);
	if ((bResult < 0) || (ack != 0x90)) {
		fprintf(stderr, "%s (%d) - NUC_ReadPipe failed %d.\n", __func__, id, __LINE__);
		goto EXIT;
	}
	return 0;
EXIT:
	return -1;
}

static void dump_info(INFO_T *info)
{
	fprintf(stdout, "Nand_uPagePerBlock    = %d\n", info->Nand_uPagePerBlock);
	fprintf(stdout, "Nand_uPageSize        = %d\n", info->Nand_uPageSize);
	fprintf(stdout, "Nand_uSectorPerBlock  = %d\n", info->Nand_uSectorPerBlock);
	fprintf(stdout, "Nand_uBlockPerFlash   = %d\n", info->Nand_uBlockPerFlash);
	fprintf(stdout, "Nand_uBadBlockCount   = %d\n", info->Nand_uBadBlockCount);
	fprintf(stdout, "Nand_uSpareSize       = %d\n", info->Nand_uSpareSize);
	fprintf(stdout, "Nand_uIsUserConfig    = %d\n", info->Nand_uIsUserConfig);

	fprintf(stdout, "SPI_ID                = 0x%x\n", info->SPI_ID);
	fprintf(stdout, "SPI_uIsUserConfig     = %d\n", info->SPI_uIsUserConfig);
	fprintf(stdout, "SPI_QuadReadCmd       = %d\n", info->SPI_QuadReadCmd);
	fprintf(stdout, "SPI_ReadStatusCmd     = %d\n", info->SPI_ReadStatusCmd);
	fprintf(stdout, "SPI_WriteStatusCmd    = %d\n", info->SPI_WriteStatusCmd);
	fprintf(stdout, "SPI_StatusValue       = %d\n", info->SPI_StatusValue);
	fprintf(stdout, "SPI_dummybyte         = %d\n", info->SPI_dummybyte);

	fprintf(stdout, "EMMC_uBlock           = %d\n", info->EMMC_uBlock);
	fprintf(stdout, "EMMC_uReserved        = %d\n", info->EMMC_uReserved);

	fprintf(stdout, "SPINand_uIsUserConfig = %d\n", info->SPINand_uIsUserConfig);
	fprintf(stdout, "SPINand_ID            = 0x%x\n", info->SPINand_ID);
	fprintf(stdout, "SPINand_PageSize      = %d\n", info->SPINand_PageSize);
	fprintf(stdout, "SPINand_SpareArea     = %d\n", info->SPINand_SpareArea);
	fprintf(stdout, "SPINand_QuadReadCmd   = %d\n", info->SPINand_QuadReadCmd);

	fprintf(stdout, "SPINand_ReadStatusCmd = %d\n", info->SPINand_ReadStatusCmd);
	fprintf(stdout, "SPINand_WriteStatusCmd= 0x%x\n", info->SPINand_WriteStatusCmd);
	fprintf(stdout, "SPINand_StatusValue   = %d\n", info->SPINand_StatusValue);
	fprintf(stdout, "SPINand_dummybyte     = %d\n", info->SPINand_dummybyte);
	fprintf(stdout, "SPINand_IsDieSelect   = %d\n", info->SPINand_IsDieSelect);
}


int Proc_Run_One_Job(int id)
{
	BOOLEAN	bSuccess = false, bResult;
	int		retrycnt;

	if (_nudata.firmware_update	== FALSE) {
		
		for (retrycnt = 1; retrycnt <= USB_CONN_RETRY_MAX; retrycnt++) {
			
			Sleep(200);	//for Mass production
			bResult = FWDownload(id);
			Sleep(200);	//for Mass production
			
			if (bResult == FALSE) {
				fprintf(stderr, "T%d: Download FW failed, retry %d\n", id, retrycnt);
				continue;
			}
			else
				fprintf(stdout, "T%d: Download FW PASS\n", id);
    	
			bResult = WinUsb_ResetFW(id);
			Sleep(100);  //for Mass production
			
			if (bResult == FALSE) {
				fprintf(stderr, "T%d: RESET NUC980 failed, retry: %d\n", id, retrycnt);
				continue;
			}
			else {
				fprintf(stdout, "T%d: RESET NUC980 FW PASS\n", id);
				break;
			}
		}
		if (retrycnt >= USB_CONN_RETRY_MAX)
			return -1;
	}
	
	if (_nudata.mode.id != MODE_SD) {
		if (InfoFromDevice(id) != 0) {
			fprintf(stderr, "T%d: InfoFromDevice failed!\n", id);
			return -1;
		}
		// dump_info(&(_nudata.user_def));
	}

	switch(_nudata.mode.id) {
	case MODE_SDRAM:
		if (UXmodem_SDRAM(id) < 0)
			return -1;
		break;
	case MODE_NAND:
		if (UXmodem_NAND(id) < 0)
			return -1;
		break;
	case MODE_SPINOR:
		if (UXmodem_SPINOR(id)<0)
			return -1;
		break;
		
	case MODE_SD:
		if (UXmodem_SD(id) < 0)
			return -1;
		break;
	case MODE_SPINAND:
		if (UXmodem_SPINAND(id) < 0)
			return -1;
		break;
	}

	if (_nudata.firmware_update == TRUE) {
		printf("\n\nReset NUC980...\n");
		WinUsb_ResetFW(id);
		CloseWinUsbDevice(id);
		return 0;
	}
	
	if (_WinUsb.WinUsbHandle[id].HandlesOpen == TRUE)
		CloseWinUsbDevice(id);
	
	return 0;
}

int main(int argc, char **argv)
{
	int		work_dev_cnt;
	int		ret;
	BOOL	bResult;
	DWORD   ticks;
	
	ticks = GetTickCount(); 
	
	if (argc != 2) {
		print_usage();
		return ERR_CODE_INI_FILE;
	}
	memset(&_nudata, 0, sizeof(_nudata));
	memset(&_devctrl, 0, sizeof(_devctrl));
	memset(&_pack, 0, sizeof(_pack));
	memset(&_WinUsb, 0, sizeof(_WinUsb));
	memset(&_mmc_head, 0, sizeof(_mmc_head));
	
	ret = ParseIniFile(argv[1]);
	if (ret != 0) {
		fprintf(stderr, "Errors on parsing .ini file.\n");
		return ret;
	}
	
	ret = LoadDDRInit(_nudata.ddr_path, _nudata.ddr_sbuff, &(_nudata.ddr_sbuff_len));
	if (ret != 0) {
		fprintf(stderr, "Errors on loading ddr.ini file.\n");
		return ret;
	}

	if (_pack.enable_pack == 1) {
		fprintf(stdout, "UXmodem_PackImage\n");
		return UXmodem_PackImage(0);
	} else {
		bResult = EnableWinUsbDevice();
		if (!bResult) {
			fprintf(stderr, "USB connect failed! ERR: %d\n", GetLastError());
			return ERR_CODE_USB_CONN;
		}

#if 1 /* Reset NUC980 anyway. If NUC980 is running xusb, this can make it return to IBR. */
		bResult = WinUsb_ResetFW(0);
		if (!bResult) {
			fprintf(stderr, "failed to reset USB decide! ERR: %d\n", GetLastError());
			return ERR_CODE_USB_CONN;
		}
		Sleep(1000);

		bResult = EnableWinUsbDevice();
		if (!bResult) {
			fprintf(stderr, "USB connect failed! ERR: %d\n", GetLastError());
			return ERR_CODE_USB_CONN;
		}
#endif
		
		if (_WinUsb.WinUsbNumber == 0) {
			fprintf(stderr, "USB device not found! %d\n", GetLastError());
			return ERR_CODE_USB_NODEV;
		}
		
		fprintf(stdout, "%d USB device connected.\n", _WinUsb.WinUsbNumber);
		
		if (_devctrl.enable_all_device == 1) {
			fprintf(stdout, "Program to all USB device. [%d]\n", _WinUsb.WinUsbNumber);
			work_dev_cnt = _WinUsb.WinUsbNumber;
		}
		else {
			fprintf(stdout, "Program to the first found USB device only. (%d)\n", _WinUsb.WinUsbNumber);
			work_dev_cnt = 1;
		}
		
		ret = Proc_Run_One_Job(0);
	}
	
	if (ret == 0) {
		ticks = GetTickCount() - ticks;
		fprintf(stdout, "\n\nNucWriter run successful. Time used: %d.%03d seconds\n\n", ticks / 1000, ticks % 1000);
	} else {
		fprintf(stderr, "\n\nNucWriter failed!! return code: %d\n\n", ret);
	}

	return ret;
}
