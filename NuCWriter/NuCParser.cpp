// NuCWriter.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <windows.h>
#include <stdio.h>
#include <fcntl.h>
#include <io.h>
#include <iostream>
#include <fstream>

#include "NuCWriter.h"
#include "NucWinUsb.h"

MODE_T ModeT[]= {
	{MODE_SDRAM,	(char *)"SDRAM"		},
	{MODE_NAND,		(char *)"NAND"		},
	{MODE_SPINOR,	(char *)"SPINOR"	},
	{MODE_SPINAND,	(char *)"SPINAND"	},
	{MODE_SD,		(char *)"SD"		},
	{0,				(char *)""			},
};

MODE_T TypeT[]= {
	{IMG_T_DATA,	(char *)"DATA"		},
	{IMG_T_ENV,		(char *)"ENV"		},
	{IMG_T_LOADER,	(char *)"LOADER"	},
	{IMG_T_PACK,	(char *)"PACK"		},
	{0,				(char *)""			},
};

MODE_T RunT[]= {
	{RUN_PROGRAM,	(char *)"PROGRAM"	},
	{RUN_PROGRAM_VERIFY, (char *)"PROGRAM_VERIFY"},
	{RUN_READ,		(char *)"READ"		},
	{RUN_ERASE,		(char *)"ERASE"		},
	{RUN_FORMAT,	(char *)"FORMAT"	},
	{0,				(char *)""			},
};

static int  ParseImageInfo(char *inifile);
static int  ParseSdramInfo(char *inifile);
static void ParseUserDefinedParam(char *inifile);

/* Using globel varible, _nudata */
int ParseIniFile(char *inifile)
{
	MODE_T   *mode;
	char     str[MAX_PATH];
	long     n;
	int      ret;

	/*---------------------------------------------------*/ 
	/* Parsing [RUN]                                     */
	/*---------------------------------------------------*/
	n = GetPrivateProfileString("RUN", "pack", "dummy", str, sizearray(str), inifile);
	if (n == 3) {
		_pack.enable_pack = 1;
		n = GetPrivateProfileString("RUN", "pack_path", "dummy", str, sizearray(str), inifile);
		strcpy_s(_pack.pack_path, MAX_PATH, str);
	}

	n = GetPrivateProfileString("RUN", "all_device", "dummy", str, sizearray(str), inifile);
	if (n == 3)
		_devctrl.enable_all_device = 1;
		
	_nudata.firmware_update = FALSE;
	n = GetPrivateProfileString("RUN", "firmware_update", "dummy", str, sizearray(str), inifile);
	if (n == 3)
		_nudata.firmware_update = TRUE;

	n = GetPrivateProfileString("RUN", "mode", "dummy", str, sizearray(str), inifile);
	mode = (MODE_T *)ModeT;
	while ((mode->pName[0] != '\0') && (n > 1)) {
		if (!_stricmp(str, mode->pName))
			break;
		else
			mode++;
	}

	n = GetPrivateProfileString("RUN", "mode", "dummy", str, sizearray(str), inifile);
	mode = (MODE_T *)ModeT;
	while ((mode->pName[0] != '\0') && (n > 1)) {
		if (!_stricmp(str, mode->pName))
			break;
		else
			mode++;
	}

	if ((mode->pName[0] == '\0') || (n == 0)) {
		fprintf(stderr, "Check [RUN] mode setting\n");
		return ERR_CODE_INI_FILE;
	} else {
		_nudata.mode.id = mode->id;
		strcpy_s(_nudata.mode.pName, mode->pName);
		fprintf(stdout, "id=%d, mode=%s\n",_nudata.mode.id, _nudata.mode.pName);
	}
	n = GetPrivateProfileString("DDR", "ddr", "dummy", _nudata.ddr_file_name, 
								sizeof(_nudata.ddr_file_name), inifile);
	sprintf_s(_nudata.ddr_path, MAX_PATH, "%s/sys_cfg/%s", NUDATA_PATH, _nudata.ddr_file_name);
	fprintf(stdout, "DDR ini = %s\n", _nudata.ddr_path);

	if (_nudata.mode.id == SDRAM) {
		/* 
		 * Parsing [SDRAM] 
		 */
		ParseSdramInfo(inifile);
	} else {
		/* 
		 * Parsing [SPINAND],[SPINOR],[SD],[NAND] 
		 */
		n = GetPrivateProfileString(ModeT[_nudata.mode.id].pName, "run", "dummy", str, sizearray(str), inifile);
		mode = (MODE_T *)RunT;
		while ((mode->pName[0] != '\0') && (n > 1)) {
			if (!_stricmp(str, mode->pName))
				break;
			else
				mode++;
		}
		_nudata.run = mode->id;
		
		ret = ParseImageInfo(inifile);
		if (ret != 0)
			return ret;

		if (_nudata.mode.id == MODE_SD) {
			n = GetPrivateProfileInt(ModeT[_nudata.mode.id].pName, "reserved_size", -1, inifile);
			_nudata.user_def.EMMC_uReserved = n;
		}

			/* Parsing User defined */
		n = GetPrivateProfileString(ModeT[_nudata.mode.id].pName, "using_user_defined", "dummy", str, sizearray(str), inifile);
		if (_stricmp(str, "yes") == 0)
			ParseUserDefinedParam(inifile);

		if (_nudata.run == RUN_READ) {
			n = GetPrivateProfileString(ModeT[_nudata.mode.id].pName, "read_path", "dummy", str, sizearray(str), inifile);
			strcpy_s(_nudata.read.path, MAX_PATH, str);

			n = GetPrivateProfileInt(ModeT[_nudata.mode.id].pName, "read_start_blocks", -1, inifile);
			_nudata.read.start_blocks = n;

			n = GetPrivateProfileInt(ModeT[_nudata.mode.id].pName, "read_offset_blocks", -1, inifile);
			_nudata.read.offset_blocks = n;
		}

		if (_nudata.run == RUN_ERASE) {
			n = GetPrivateProfileString(ModeT[_nudata.mode.id].pName, "erase_all", "dummy", str, sizearray(str), inifile);
			if (n == 2) {
				n = GetPrivateProfileInt(ModeT[_nudata.mode.id].pName, "erase_start_blocks", -1, inifile);
				_nudata.erase.start_blocks = n;
				n = GetPrivateProfileInt(ModeT[_nudata.mode.id].pName, "erase_offset_blocks", -1, inifile);
				_nudata.erase.offset_blocks = n;
			} else
				_nudata.erase.offset_blocks = 0xffffffff;

			printf("%s RUN_ERASE, %d ~ %d\n", ModeT[_nudata.mode.id].pName, _nudata.erase.start_blocks, _nudata.erase.offset_blocks);
		}
	}
	fprintf(stdout, "ParsingIni END\n");
	return 0;
}

static int ParseSdramInfo(char *inifile)
{
	char     str[MAX_PATH];
	long     n;

	n = GetPrivateProfileString(ModeT[_nudata.mode.id].pName, "sdram_path", "dummy", str, sizearray(str), inifile);
	strcpy_s(_nudata.sdram.sdram_path, MAX_PATH, str);

	n = GetPrivateProfileString(ModeT[_nudata.mode.id].pName, "option", "dummy", str, sizearray(str), inifile);
	if (n == 3)
		_nudata.sdram.opetion = DOWNLOAD_RUN;
	else
		_nudata.sdram.opetion=DOWNLOAD;
	
	n = GetPrivateProfileInt(ModeT[_nudata.mode.id].pName, "exe_addr", -1, inifile);
	_nudata.sdram.exe_addr = n;
	
	n = GetPrivateProfileString(ModeT[_nudata.mode.id].pName, "using_dtb", "dummy", str, sizearray(str), inifile);
	if (n == 2) {
		_nudata.sdram.dtb_addr = 0;
	} else {
		n = GetPrivateProfileString(ModeT[_nudata.mode.id].pName,
						"dtb_path", "dummy", str, sizearray(str), inifile);
		strcpy_s(_nudata.sdram.dtb_path, MAX_PATH, str);
		n = GetPrivateProfileInt(ModeT[_nudata.mode.id].pName, "dtb_addr", -1, inifile);
		_nudata.sdram.dtb_addr  = n;
	}
	fprintf(stdout, "_nudata.sdram.exe_addr=0x%08x\n", _nudata.sdram.exe_addr);
	fprintf(stdout, "_nudata.sdram.dtb_addr=0x%08x\n", _nudata.sdram.dtb_addr);
	return 0;
}

static int ParseImageInfo(char *inifile)
{
	MODE_T   *mode;
	char     str[MAX_PATH], str2[MAX_PATH];
	long     n;
	int      i, idx;

	n = GetPrivateProfileInt(ModeT[_nudata.mode.id].pName, "image_num", -1, inifile);
	_nudata.image_num = n;
	if (_nudata.image_num == 0) {
		fprintf(stderr,"Set image number\n");
		return ERR_CODE_INI_FILE;
	}
	fprintf(stdout, "image_num = %d\n", n);
		
	for (i = 0; i < _nudata.image_num; i++) {
		sprintf_s(str2, sizeof(str2), "image%d_type", i);
		n = GetPrivateProfileString(ModeT[_nudata.mode.id].pName, str2, "dummy", str, sizearray(str), inifile);
		mode = (MODE_T *)TypeT;
		idx = 0;
		while ((mode->pName[0] != '\0') && (n > 1)) {
			if (!_stricmp(str, mode->pName)) {
				break;
			} else {
				mode++;
				idx++;
			}
		}
		if (mode->pName[0] == '\0') {
			fprintf(stderr,"Cannot find image type\n");
			fprintf(stdout, "============================>Cannot find image type\n");
			return ERR_CODE_OPEN_FILE_IMG;
		}

		_nudata.image[i].image_idx = idx;
		_nudata.image[i].image_type = mode->id;
		sprintf_s(str2, sizeof(str2), "image%d_path",i);
		n = GetPrivateProfileString(ModeT[_nudata.mode.id].pName, str2, "dummy", str, sizearray(str), inifile);
		if (n == 0) {
			fprintf(stderr,"Cannot find image%d\n",i);
			return ERR_CODE_OPEN_FILE_IMG;
		}
		if (_access(str, 0) != 0) {
			fprintf(stderr,"Cannot access image%d [path:%s]\n", i, str);
			return ERR_CODE_OPEN_FILE_IMG;
		}
		strcpy_s(_nudata.image[i].image_path, MAX_PATH, str);

		sprintf_s(str2, sizeof(str2), "image%d_exe_addr", i);
		n = GetPrivateProfileInt(ModeT[_nudata.mode.id].pName, str2, -1, inifile);
		_nudata.image[i].image_exe_addr = n;

		sprintf_s(str2, sizeof(str2), "image%d_start_offset", i);
		n = GetPrivateProfileInt(ModeT[_nudata.mode.id].pName, str2, -1, inifile);
		_nudata.image[i].image_start_offset = n;
	}

	// fprintf(stdout, "_nudata.image_num=%d\n", _nudata.image_num);
	for (i = 0; i < _nudata.image_num; i++) {
		fprintf(stdout, "image%d_type=%d\n", i, _nudata.image[i].image_type);
		fprintf(stdout, "image%d_path=%s\n", i, _nudata.image[i].image_path);
		fprintf(stdout, "image%d_exe_addr=0x%08x\n", i, _nudata.image[i].image_exe_addr);
		fprintf(stdout, "image%d_start_offset=0x%08x\n", i, _nudata.image[i].image_start_offset);
	}
	return 0;
}

static void ParseUserDefinedParam(char *inifile)
{
	char     str[MAX_PATH];
	long     n;

	_nudata.user_def.EMMC_uReserved = 1000 ;
	_nudata.user_def.Nand_uBlockPerFlash = 1024;
	_nudata.user_def.Nand_uPagePerBlock =	64;

	if (_nudata.mode.id == MODE_SD) {
		n = GetPrivateProfileInt(ModeT[_nudata.mode.id].pName, "reserved_size", -1, inifile);
		_nudata.user_def.EMMC_uReserved = n ;

		n = GetPrivateProfileString(ModeT[_nudata.mode.id].pName, "using_format", "dummy", str, sizearray(str), inifile);
		if (n == 2) {
			_mmc_head.PartitionNum = 0 ;
		} else {
			n = GetPrivateProfileInt(ModeT[_nudata.mode.id].pName, "partition_num", -1, inifile);
			_mmc_head.PartitionNum = n ;
		}

		n = GetPrivateProfileInt(ModeT[_nudata.mode.id].pName, "partition1_size", -1, inifile);
		_mmc_head.Partition1Size = n ;

		n = GetPrivateProfileInt(ModeT[_nudata.mode.id].pName, "partition2_size", -1, inifile);
		_mmc_head.Partition2Size = n ;

		n = GetPrivateProfileInt(ModeT[_nudata.mode.id].pName, "partition3_size", -1, inifile);
		_mmc_head.Partition3Size = n ;

		_mmc_head.Partition4Size = 0 ;

	} else if (_nudata.mode.id == MODE_NAND) {

		n = GetPrivateProfileInt(ModeT[_nudata.mode.id].pName, "blockperflash", -1, inifile);
		_nudata.user_def.Nand_uBlockPerFlash = n;

		n = GetPrivateProfileInt(ModeT[_nudata.mode.id].pName, "pageperblock", -1, inifile);
		_nudata.user_def.Nand_uPagePerBlock = n;

		_nudata.user_def.Nand_uIsUserConfig =1;
	} else if(_nudata.mode.id==MODE_SPINAND) {

		n = GetPrivateProfileInt(ModeT[_nudata.mode.id].pName, "page_size", -1, inifile);
		_nudata.user_def.SPINand_PageSize = (UINT16)n;

		n = GetPrivateProfileInt(ModeT[_nudata.mode.id].pName, "spare_area", -1, inifile);
		_nudata.user_def.SPINand_SpareArea = (UINT16)n;

		n = GetPrivateProfileInt(ModeT[_nudata.mode.id].pName, "quad_read_command", -1, inifile);
		_nudata.user_def.SPINand_QuadReadCmd = (UINT8)n;

		n = GetPrivateProfileInt(ModeT[_nudata.mode.id].pName, "read_status_command", -1, inifile);
		_nudata.user_def.SPINand_ReadStatusCmd = (UINT8)n;

		n = GetPrivateProfileInt(ModeT[_nudata.mode.id].pName, "write_status_command", -1, inifile);
		_nudata.user_def.SPINand_WriteStatusCmd = (UINT8)n;

		n = GetPrivateProfileInt(ModeT[_nudata.mode.id].pName, "status_value", -1, inifile);
		_nudata.user_def.SPINand_StatusValue = (UINT8)n;

		n = GetPrivateProfileInt(ModeT[_nudata.mode.id].pName, "dummy_bytes", -1, inifile);
		_nudata.user_def.SPINand_dummybyte = (UINT8)n;

		n = GetPrivateProfileInt(ModeT[_nudata.mode.id].pName, "blockperflash", -1, inifile);
		_nudata.user_def.SPINand_BlockPerFlash = (UINT8)n;

		n = GetPrivateProfileInt(ModeT[_nudata.mode.id].pName, "pageperflash", -1, inifile);
		_nudata.user_def.SPINand_PagePerBlock = n;

		n = GetPrivateProfileInt(ModeT[_nudata.mode.id].pName, "multichip", -1, inifile);
		_nudata.user_def.SPINand_IsDieSelect = (UINT8)n;

		_nudata.user_def.SPINand_uIsUserConfig =1;

	} else if(_nudata.mode.id==MODE_SPINOR) {

		n = GetPrivateProfileInt(ModeT[_nudata.mode.id].pName, "quad_read_command", -1, inifile);
		_nudata.user_def.SPI_QuadReadCmd = (UINT8)n;

		n = GetPrivateProfileInt(ModeT[_nudata.mode.id].pName, "read_status_command", -1, inifile);
		_nudata.user_def.SPI_ReadStatusCmd = (UINT8)n;

		n = GetPrivateProfileInt(ModeT[_nudata.mode.id].pName, "write_status_command", -1, inifile);
		_nudata.user_def.SPI_WriteStatusCmd = (UINT8)n;

		n = GetPrivateProfileInt(ModeT[_nudata.mode.id].pName, "status_value", -1, inifile);
		_nudata.user_def.SPI_StatusValue = (UINT8)n;

		n = GetPrivateProfileInt(ModeT[_nudata.mode.id].pName, "dummy_bytes", -1, inifile);
		_nudata.user_def.SPI_dummybyte = (UINT8)n;

		_nudata.user_def.SPI_uIsUserConfig =1;
	}
}
