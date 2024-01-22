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

#define MIN(a,b) ((a) < (b) ? (a) : (b))

int DataCompare(unsigned char* base, unsigned char* src, int len)
{
	int i = 0;
	
	for (i = 0; i < len; i++) {
		if (base[i] != src[i])
			return 0;
	}
	return 1;
}

unsigned char *GetDDRFormat(unsigned int *len)
{
	unsigned char	*dbuf, *ddrbuf;
	unsigned int	dlen;

	dbuf = (unsigned char *)_nudata.ddr_sbuff;
	dlen = _nudata.ddr_sbuff_len;

	*len = ((dlen + 8 + 15) / 16) * 16;
	ddrbuf = (unsigned char *)malloc(sizeof(unsigned char)*(*len));
	memset(ddrbuf, 0x0, *len);
	*(ddrbuf + 0) = 0x55;
	*(ddrbuf + 1) = 0xAA;
	*(ddrbuf + 2) = 0x55;
	*(ddrbuf + 3) = 0xAA;
	*((unsigned int *)(ddrbuf + 4)) = (dlen / 8);        /* len */
	memcpy((ddrbuf + 8), dbuf, dlen);
	return ddrbuf;
}

int UXmodem_PackImage(int id)
{
	int		storageSize = (64 * 1024);
	int		idx, tmp;
	PACK_HEAD pack_head;
	FILE	*wfp, *rfp = NULL;
	int		total = 0;
	unsigned int file_len;
	PACK_CHILD_HEAD child;
	unsigned int len;
	
	if (fopen_s(&wfp, _pack.pack_path, "w+b") != 0) {
		fprintf(stderr, "%s %d - File Open error\n", __func__, __LINE__);
		return -1;
	}
	for (idx = 0; idx < _nudata.image_num; idx++) {
		if (fopen_s(&rfp, _nudata.image[idx].image_path, "rb") != 0) {
			fprintf(stderr, "%s %d - Open read File Error(-w %s) \n",
						__func__, __LINE__, _nudata.image[idx].image_path);
			return -1;
		}
		fseek(rfp, 0, SEEK_END);
		file_len = ftell(rfp);
		fclose(rfp);
		rfp = NULL;
		total += file_len;
	}
	memset((char *)&pack_head, 0xff, sizeof(pack_head));
	pack_head.actionFlag = PACK_ACTION;
	pack_head.fileLength = total;
	pack_head.num = _nudata.image_num;

	if ((_mmc_head.PartitionNum != 0) && (_nudata.mode.id == MODE_SD))
		pack_head.num++;

	//write  pack_head
	fwrite((char *)&pack_head, sizeof(PACK_HEAD), 1, wfp);

	for (idx = 0; idx < _nudata.image_num; idx++) {

		//if(*itemType!=PMTP) {
		if (1) {
			//itemStartblock=(ImageStartblock.begin()+i);
			if (_nudata.image[idx].image_type != IMG_T_PARTITION) {
				fopen_s(&rfp, _nudata.image[idx].image_path, "rb");
				fseek(rfp, 0, SEEK_END);
				len= ftell(rfp);
				fseek(rfp, 0, SEEK_SET);
			} else {
				len = PACK_FOMRAT_HEADER_LEN; // partition header length
			}

			char *pBuffer=NULL;
			char magic[4]= {' ','T','V','N'};
			
			switch (_nudata.image[idx].image_type) {
			case IMG_T_LOADER: {
				unsigned int ddrlen;
				unsigned char *ddrbuf;

				ddrbuf = GetDDRFormat(&ddrlen);
				memset((char *)&child, 0xff, sizeof(PACK_CHILD_HEAD));

				//write  pack_child_head
				child.filelen = len + ddrlen + 32;
				child.startaddr = _nudata.image[idx].image_start_offset;
				child.imagetype = IMG_T_LOADER;
				fwrite((char *)&child, 1, sizeof(PACK_CHILD_HEAD), wfp);

				//write uboot head
				fwrite((char *)magic, 1, sizeof(magic), wfp);
				//_stscanf_s(*itemExec,_T("%x"),&tmp);
				fwrite((char *)&_nudata.image[idx].image_exe_addr, 1, 4, wfp);
				fwrite((char *)&len, 1, 4, wfp);
				tmp = 0xffffffff;
				fwrite((char *)&tmp, 1, 4, wfp);
#if(1)
				//Add IBR header for NUC980 SPI NOR/NAND
				if (_nudata.mode.id == MODE_SPINAND) { //SPI NAND
					tmp = _nudata.user_def.SPINand_PageSize;
					fwrite((char *)&tmp, 1, 2, wfp);
					tmp = _nudata.user_def.SPINand_SpareArea;
					fwrite((char *)&tmp, 1, 2, wfp);
					tmp = _nudata.user_def.SPINand_QuadReadCmd;
					fwrite((char *)&tmp, 1, 1, wfp);
					tmp = _nudata.user_def.SPINand_ReadStatusCmd;
					fwrite((char *)&tmp, 1, 1, wfp);
					tmp =_nudata.user_def.SPINand_WriteStatusCmd;
					fwrite((char *)&tmp, 1, 1, wfp);
					tmp = _nudata.user_def.SPINand_StatusValue;
					fwrite((char *)&tmp, 1, 1, wfp);
					tmp = _nudata.user_def.SPINand_dummybyte;
					fwrite((char *)&tmp, 1, 1, wfp);
					tmp = 0xffffff;
					fwrite((char *)&tmp, 1, 3, wfp);
					tmp = 0xffffffff;
					fwrite((char *)&tmp, 1, 4, wfp);
				} else //SPI NOR
#endif
				{
					tmp = 0x800;
					fwrite((char *)&tmp, 1, 2, wfp);
					tmp = 0x40;
					fwrite((char *)&tmp, 1, 2, wfp);
					tmp = _nudata.user_def.SPI_QuadReadCmd;
					fwrite((char *)&tmp, 1, 1, wfp);
					tmp = _nudata.user_def.SPI_ReadStatusCmd;
					fwrite((char *)&tmp, 1, 1, wfp);
					tmp = _nudata.user_def.SPI_WriteStatusCmd;
					fwrite((char *)&tmp, 1, 1, wfp);
					tmp = _nudata.user_def.SPI_StatusValue;
					fwrite((char *)&tmp, 1, 1, wfp);
					tmp = _nudata.user_def.SPI_dummybyte;
					fwrite((char *)&tmp, 1, 1, wfp);
					tmp = 0xffffff;
					fwrite((char *)&tmp, 1, 3, wfp);
					tmp = 0xffffffff;
					fwrite((char *)&tmp, 1, 4, wfp);
				}

				//write DDR
				fwrite(ddrbuf, 1, ddrlen, wfp);

				pBuffer = (char *)malloc(len);

				if (rfp == NULL) {
					fprintf(stderr, "%s %d - rfp is NULL!\n", __func__, __LINE__);
					fclose(wfp);
					return -1;
				}
				fread(pBuffer, 1, len, rfp);
				fwrite((char *)pBuffer, 1, len, wfp);
			}
			break;
			case IMG_T_ENV  :
				memset((char *)&child, 0xff, sizeof(PACK_CHILD_HEAD));
				if (_nudata.mode.id == MODE_SPINAND) {//spi nand
					child.filelen = 0x20000;
					child.imagetype = IMG_T_ENV;
					pBuffer = (char *)malloc(0x20000);
					memset(pBuffer, 0x0, 0x20000);
				} else {
					child.filelen = 0x10000;
					child.imagetype = IMG_T_ENV;
					pBuffer = (char *)malloc(0x10000);
					memset(pBuffer, 0x0, 0x10000);
				}
				child.startaddr = _nudata.image[idx].image_start_offset;
				
				//-----------------------------------------------
				fwrite((char *)&child, sizeof(PACK_CHILD_HEAD), 1, wfp);
				{
					char	line[256];
					char	*ptr=(char *)(pBuffer + 4);
					while (1) {
						if (fgets(line, 256, rfp) == NULL)
							break;
						if ((line[strlen(line) - 2] == 0x0D) && (line[strlen(line) - 1] == 0x0A)) {
							strncpy_s(ptr, 0x20000, line, strlen(line) - 1);
							ptr[strlen(line) - 2] = 0x0;
							ptr += (strlen(line) - 1);
						} else if (line[strlen(line) - 1] == 0x0A) {
							strncpy_s(ptr, 0x20000, line, strlen(line));
							ptr[strlen(line) - 1] = 0x0;
							ptr += strlen(line);
						} else {
							strncpy_s(ptr, 0x20000, line, strlen(line));
							ptr += strlen(line);
						}
					}
				}
				if (_nudata.mode.id == MODE_SPINAND) {//spi nand
					*(unsigned int *)pBuffer = CalculateCRC32((unsigned char *)(pBuffer + 4), 0x20000 - 4);
					fwrite((char *)pBuffer, 1, 0x20000, wfp);
				} else {
					*(unsigned int *)pBuffer = CalculateCRC32((unsigned char *)(pBuffer + 4), 0x10000 - 4);
					fwrite((char *)pBuffer, 1, 0x10000, wfp);
				}
				break;
			
			case IMG_T_DATA : {
				memset((char *)&child, 0xff, sizeof(PACK_CHILD_HEAD));
				child.filelen = len;
				child.imagetype = IMG_T_DATA;
				pBuffer = (char *)malloc(child.filelen);
				child.startaddr = _nudata.image[idx].image_start_offset;
				//-----------------------------------------------
				fwrite((char *)&child, sizeof(PACK_CHILD_HEAD), 1, wfp);
				fread(pBuffer, 1, len, rfp);
				fwrite((char *)pBuffer, 1, len, wfp);
			}
			break;
			
			case IMG_T_IMAGE:
				memset((char *)&child, 0xff, sizeof(PACK_CHILD_HEAD));
				child.filelen = len;
				child.imagetype = IMG_T_IMAGE;
				pBuffer = (char *)malloc(child.filelen);
				child.startaddr = _nudata.image[idx].image_start_offset;
				//-----------------------------------------------
				fwrite((char *)&child, sizeof(PACK_CHILD_HEAD), 1, wfp);
				fread(pBuffer, 1, len, rfp);
				fwrite((char *)pBuffer, 1, len, wfp);
				break;
			}
			fclose(rfp);
			if (pBuffer != NULL)
				free(pBuffer);
		}
	}

	if ((_mmc_head.PartitionNum != 0) && (_nudata.mode.id == MODE_SD)) {
		memset((char *)&child, 0xff, sizeof(PACK_CHILD_HEAD));
		child.filelen= PACK_FOMRAT_HEADER_LEN; // partition header length
		child.imagetype = IMG_T_PARTITION;
		fwrite((char *)&child, 1, sizeof(PACK_CHILD_HEAD), wfp);
		tmp = 0;
		fwrite((char *)&tmp, 1, 4, wfp);
		tmp= _mmc_head.PartitionNum;
		fwrite((char *)&tmp, 1, 4, wfp);
		tmp = _nudata.user_def.EMMC_uReserved;
		fwrite((char *)&tmp, 1, 4, wfp);
		tmp = 0xffffffff;
		fwrite((char *)&tmp, 1, 4, wfp);
		tmp=_mmc_head.Partition1Size;
		fwrite((char *)&tmp, 1, 4, wfp);
		tmp = tmp * 2 * 1024;  //PartitionS1Size
		fwrite((char *)&tmp, 1, 4, wfp);
		tmp=_mmc_head.Partition2Size;
		fwrite((char *)&tmp, 1, 4, wfp);
		tmp = tmp * 2 * 1024;  //PartitionS2Size
		fwrite((char *)&tmp, 1, 4, wfp);
		tmp=_mmc_head.Partition3Size;
		fwrite((char *)&tmp, 1, 4, wfp);
		tmp = tmp * 2 * 1024;  //PartitionS3Size
		fwrite((char *)&tmp, 1, 4, wfp);
		tmp= _mmc_head.Partition4Size;
		fwrite((char *)&tmp, 1, 4, wfp);
		tmp = tmp * 2 * 1024;  //PartitionS4Size
		fwrite((char *)&tmp, 1, 4, wfp);
	}
	fclose(wfp);
	fprintf(stdout, "Output finish\n");
	return 0;
}

int UXmodem_DTB(int id)
{
	FILE	*fp = NULL;
	int		bResult, pos;
	unsigned int	scnt, rcnt, file_len, ack,total;
	unsigned char	*pbuf,buf[BUF_SIZE];
	SDRAM_RAW_TYPEHEAD fhead;
	USHORT	typeack;

	NUC_SetType(0, SDRAM, (UCHAR *)&typeack, sizeof(typeack));

	fprintf(stdout, "dtb_path = %s\n", _nudata.sdram.dtb_path);
	if (fopen_s(&fp, _nudata.sdram.dtb_path, "rb") != 0) {
		fprintf(stderr, "%s (%d) - Open write File Error(%s) \n", __func__, id, _nudata.sdram.dtb_path);
		goto EXIT;
	}
	fseek(fp, 0, SEEK_END);
	file_len = ftell(fp);
	fseek(fp, 0, SEEK_SET);

	if (!file_len) {
		fclose(fp);
		fprintf(stderr, "%s (%d) File length is zero\n", __func__, id);
		goto EXIT;
	}

	fprintf(stdout, "dtb_addr = %x\n", _nudata.sdram.dtb_addr);
	fhead.flag = WRITE_ACTION;
	fhead.filelen = file_len;
	fhead.address = _nudata.sdram.dtb_addr;
	fhead.dtbaddress = 0;
	//if(_nudata.sdram.opetion==DOWNLOAD_RUN) {
	//	fhead.address |= NEED_AUTORUN;
	//}

	//if(_nudata.sdram.dtb_addr!=0) {
	//	fhead.dtbaddress = _nudata.sdram.dtb_addr | NEED_AUTORUN;
	//}
	bResult = NUC_WritePipe(id, (unsigned char *)&fhead, sizeof(SDRAM_RAW_TYPEHEAD));
	if (bResult == FALSE) {
		fprintf(stderr, "%s (%d) - NUC_WritePipe failed %d.\n", __func__, id, __LINE__);
		goto EXIT;
	}
	bResult = NUC_ReadPipe(id, (unsigned char *)&ack, (int)sizeof(unsigned int));
	if (bResult == FALSE) {
		fprintf(stderr, "%s (%d) - NUC_WritePipe failed %d.\n", __func__, id, __LINE__);
		goto EXIT;
	}
	pbuf = buf;
	scnt = file_len / BUF_SIZE;
	rcnt = file_len % BUF_SIZE;
	total = 0;
	while (scnt > 0) {
		fread(pbuf, 1, BUF_SIZE, fp);
		bResult = NUC_WritePipe(id, (unsigned char*)pbuf, BUF_SIZE);
		if (bResult == FALSE) {
			fprintf(stderr, "%s (%d) - NUC_WritePipe failed %d.\n", __func__, id, __LINE__);
			goto EXIT;
		}
		total += BUF_SIZE;
		pos = (int)(((float)(((float)total / (float)file_len)) * 100));
		bResult = NUC_ReadPipe(id, (UCHAR *)&ack, 4);
		if ((bResult == FALSE) || (ack != BUF_SIZE)) {
			fprintf(stderr, "%s (%d) - NUC_ReadPipe failed %d.\n", __func__, id, __LINE__);
			goto EXIT;
		}
		scnt--;
		// show_progressbar(pos);
	}
	if (rcnt > 0) {
		fread(pbuf, 1, rcnt, fp);
		bResult = NUC_WritePipe(id, (unsigned char*)pbuf, rcnt);
		if (bResult == FALSE) {
			fprintf(stderr, "%s (%d) - NUC_WritePipe failed %d.\n", __func__, id, __LINE__);
			goto EXIT;
		}
		total += rcnt;
		bResult = NUC_ReadPipe(id, (UCHAR *)&ack, 4);
		if (bResult == FALSE) {
			fprintf(stderr, "%s (%d) - NUC_ReadPipe failed %d.\n", __func__, id, __LINE__);
			goto EXIT;
		}
	}
	// show_progressbar(100);
	CloseWinUsbDevice(id);
	fclose(fp);
	return 0;
EXIT:
	CloseWinUsbDevice(id);
	if (fp != NULL)
		fclose(fp);
	fprintf(stderr, "%s (%d) failed!\n", __func__, id);
	return -1;
}

int UXmodem_SDRAM(int id)
{
	FILE	*fp = NULL;
	int		bResult, pos;
	unsigned int scnt, rcnt, file_len, ack,total;
	unsigned char *pbuf,buf[BUF_SIZE];
	SDRAM_RAW_TYPEHEAD fhead;
	USHORT	typeack;
	
	fprintf(stdout, "[%s] (%d)\n", __func__, id);

	if (EnableOneWinUsbDevice(id) == FALSE) {
		fprintf(stderr, "%s %d (%d) - Failed to enable WinUsb device!\n", __func__, __LINE__, id);
		return ERR_CODE_USB_CONN;
	}

	if (_nudata.sdram.dtb_addr != 0) {
		fprintf(stdout, "%s - Burn DTB binary files\n", __func__);
		if (UXmodem_DTB(id)!=0)
			goto EXIT;
	}

	NUC_SetType(0, SDRAM, (UCHAR *)&typeack, sizeof(typeack));

	if (fopen_s(&fp, _nudata.sdram.sdram_path, "rb") != 0) {
		fprintf(stderr, "%s (%d) - Open write File Error(-w %s) \n", __func__, id, _nudata.sdram.sdram_path);
		goto EXIT;
	}
	fseek(fp, 0, SEEK_END);
	file_len = ftell(fp);
	fseek(fp, 0, SEEK_SET);

	if (!file_len) {
		fclose(fp);
		fprintf(stderr, "%s (%d) - File length is zero\n", __func__, id);
		goto EXIT;
	}
	
	fprintf(stdout, "[%s] file: %s, len: %d\n", __func__, _nudata.sdram.sdram_path, file_len);

	fhead.flag = WRITE_ACTION;
	fhead.filelen = file_len;
	fhead.address = _nudata.sdram.exe_addr;
	fhead.dtbaddress = 0;
	if (_nudata.sdram.opetion == DOWNLOAD_RUN) {
		fhead.address |= NEED_AUTORUN;
	}

	if (_nudata.sdram.dtb_addr!=0) {
		fhead.dtbaddress = _nudata.sdram.dtb_addr | NEED_AUTORUN;
	}
	bResult = NUC_WritePipe(id, (unsigned char*)&fhead, sizeof(SDRAM_RAW_TYPEHEAD));
	if (bResult == FALSE) {
		fprintf(stderr, "%s (%d) - NUC_WritePipe failed %d.\n", __func__, id, __LINE__);
		goto EXIT;
	}
	bResult = NUC_ReadPipe(id, (unsigned char *)&ack, (int)sizeof(unsigned int));
	if (bResult == FALSE) {
		fprintf(stderr, "%s (%d) - NUC_WritePipe failed %d.\n", __func__, id, __LINE__);
		goto EXIT;
	}
	pbuf = buf;
	scnt = file_len / BUF_SIZE;
	rcnt = file_len % BUF_SIZE;
	total = 0;
	while (scnt > 0) {
		fread(pbuf, 1, BUF_SIZE, fp);
		bResult = NUC_WritePipe(id, (unsigned char*)pbuf, BUF_SIZE);
		if (bResult == FALSE)
			goto EXIT;
		total += BUF_SIZE;
		pos = (int)(((float)(((float)total / (float)file_len))*100));
		bResult = NUC_ReadPipe(id, (UCHAR *)&ack, 4);
		if ((bResult == FALSE) || (ack!=BUF_SIZE)) {
			fprintf(stderr, "%s (%d) - NUC_ReadPipe failed %d.\n", __func__, id, __LINE__);
			goto EXIT;
		}
		scnt--;
		//fprintf(stdout, "scnt = %d\n", scnt);
		// show_progressbar(pos);
	}
	if (rcnt > 0) {
		fread(pbuf, 1, rcnt,fp);
		bResult = NUC_WritePipe(id, (unsigned char*)pbuf, rcnt);
		if (bResult == FALSE) {
			fprintf(stderr, "%s (%d) - NUC_WritePipe failed %d.\n", __func__, id, __LINE__);
			goto EXIT;
		}
		total += rcnt;
		if (_nudata.sdram.opetion != DOWNLOAD_RUN) {
			bResult = NUC_ReadPipe(id, (UCHAR *)&ack, 4);
			if (bResult == FALSE) {
				fprintf(stderr, "%s (%d) - NUC_ReadPipe failed %d.\n", __func__, id, __LINE__);
				goto EXIT;
			}
		}
	}
	// show_progressbar(100);
	CloseWinUsbDevice(id);
	fclose(fp);
	fprintf(stdout, "[%s] done.\n", __func__);
	return 0;
EXIT:
	CloseWinUsbDevice(id);
	if (fp != NULL)
		fclose(fp);
	return -1;
}

int UXmodem_NAND(int id)
{
	int		idx = 0;
	FILE	*fp;
	int		bResult, pos;
	unsigned int scnt, rcnt, file_len, ack, total;
	unsigned char *pbuf;
	NORBOOT_NAND_HEAD m_fhead;
	unsigned char	*ddrbuf=NULL;
	unsigned int	ddrlen;
	unsigned char	*lpBuffer = NULL;
	int		blockNum;
	USHORT	typeack;

	if (((_nudata.user_def.Nand_uPageSize == 0) || (_nudata.user_def.Nand_uPagePerBlock == 0)) &&
		(_nudata.user_def.Nand_uIsUserConfig == 1)) {
		fprintf(stderr, "%s (%d) - Cannot find NAND(%d,%d)\n", __func__, id,
						_nudata.user_def.Nand_uPageSize,_nudata.user_def.Nand_uPagePerBlock);
		return -1;
	}

	if (EnableOneWinUsbDevice(id) == FALSE) {
		fprintf(stderr, "%s %d (%d) - Failed to enable WinUsb device!\n", __func__, __LINE__, id);
		return ERR_CODE_USB_CONN;
	}

	NUC_SetType(0, NAND, (UCHAR *)&typeack, sizeof(typeack));

	if (_nudata.image[idx].image_type == IMG_T_PACK) {
		if (UXmodem_Pack(id) != 0) {
			fprintf(stderr, "UXmodem_Pack failed!\n");
			goto EXIT;
		} else {
			return 0;
		}
	} else {

		if (_nudata.run == RUN_READ) {
			unsigned char temp[BUF_SIZE];
			
			//-----------------------------------
			if (fopen_s(&fp, _nudata.read.path, "w+b") != 0) {
				fprintf(stderr, "%s (%d) - Open write File Error(-w %s) \n", __func__, id, _nudata.read.path);
				goto EXIT;
			}
			file_len = _nudata.read.offset_blocks * _nudata.user_def.Nand_uPageSize * _nudata.user_def.Nand_uPagePerBlock;
			m_fhead.flag = READ_ACTION;
			m_fhead.flashoffset = _nudata.read.start_blocks;
			m_fhead.filelen = file_len;
			m_fhead.initSize = 0; //read good block
			bResult = NUC_WritePipe(id, (UCHAR *)&m_fhead, sizeof(NORBOOT_NAND_HEAD));
			if (bResult == FALSE) {
				fprintf(stderr, "%s (%d) - NUC_WritePipe failed %d.\n", __func__, id, __LINE__);
				goto EXIT;
			}
			bResult = NUC_ReadPipe(id, (unsigned char *)&ack, (int)sizeof(unsigned int));
			if (bResult == FALSE) {
				fprintf(stderr, "%s (%d) - NUC_ReadPipe failed %d.\n", __func__, id, __LINE__);
				goto EXIT;
			}

			scnt=file_len / BUF_SIZE;
			rcnt=file_len % BUF_SIZE;
			total = 0;
			while (scnt > 0) {
				bResult = NUC_ReadPipe(id, (unsigned char*)temp, BUF_SIZE);
				if (bResult == FALSE) {
					fprintf(stderr, "%s (%d) - NUC_ReadPipe failed %d.\n", __func__, id, __LINE__);
					fclose(fp);
					goto EXIT;
				}
				total += BUF_SIZE;
				pos=(int)(((float)(((float)total / (float)file_len)) * 100));
				fprintf(stdout, "Read ... ");
				// show_progressbar(pos);
				fwrite(temp, 1, BUF_SIZE, fp);
				ack = BUF_SIZE;
				bResult = NUC_WritePipe(id, (UCHAR *)&ack, 4);
				if (bResult == FALSE) {
					fprintf(stderr, "%s (%d) - NUC_WritePipe failed %d.\n", __func__, id, __LINE__);
					fclose(fp);
					goto EXIT;
				}
				scnt--;
			}

			if (rcnt > 0) {
				bResult = NUC_ReadPipe(id, (UCHAR *)temp, BUF_SIZE);
				if (bResult == FALSE) {
					fprintf(stderr, "%s (%d) - NUC_ReadPipe failed %d.\n", __func__, id, __LINE__);
					fclose(fp);
					goto EXIT;
				}
				total += rcnt;
				fwrite(temp, 1, rcnt, fp);
				ack = BUF_SIZE;
				bResult = NUC_WritePipe(id, (UCHAR *)&ack, 4);
				if (bResult == FALSE) {
					fprintf(stderr, "%s (%d) - NUC_WritePipe failed %d.\n", __func__, id, __LINE__);
					fclose(fp);
					goto EXIT;
				}
				pos=(int)(((float)(((float)total / (float)file_len)) * 100));
				fprintf(stdout, "Read ... ");
				// show_progressbar(pos);
			}
			fclose(fp);
			// show_progressbar(100);
			fprintf(stdout, "Read ... Passed\n");
		}

		if ((_nudata.run == RUN_PROGRAM) || (_nudata.run == RUN_PROGRAM_VERIFY)) { //Burn Image
			for (idx = 0; idx < _nudata.image_num; idx++) {
				if (idx > 0) {
					if (EnableOneWinUsbDevice(id) == FALSE) {
						fprintf(stderr, "%s %d (%d) - Failed to enable WinUsb device!\n", __func__, __LINE__, id);
						return ERR_CODE_USB_CONN;
					}
					
					NUC_SetType(0, NAND, (UCHAR *)&typeack, sizeof(typeack));
					usleep(10);
				}
				
				if (fopen_s(&fp, _nudata.image[idx].image_path, "rb") != 0) {
					fprintf(stderr, "Open read File Error(-w %s) \n",_nudata.image[idx].image_path);
					goto EXIT;
				}
				fprintf(stdout, "image%d %s\n",idx,_nudata.image[idx].image_path);
				fseek(fp, 0, SEEK_END);
				file_len = ftell(fp);
				fseek(fp, 0, SEEK_SET);

				if (!file_len) {
					fclose(fp);
					fprintf(stderr, "%s (%d) - File length is zero\n", __func__, id);
					goto EXIT;
				}
				memset(&m_fhead, 0, sizeof(NORBOOT_NAND_HEAD));
				m_fhead.flag = WRITE_ACTION;
				m_fhead.initSize = 0;
				m_fhead.filelen = file_len;
				switch(_nudata.image[idx].image_type) {
				case IMG_T_DATA:
				case IMG_T_PACK:
					m_fhead.no=0;
					m_fhead.execaddr = 0x200;
					m_fhead.flashoffset = _nudata.image[idx].image_start_offset;
					lpBuffer = (unsigned char *)malloc(sizeof(unsigned char)*file_len); //read file to buffer
					memset(lpBuffer, 0xff, file_len);
					m_fhead.macaddr[7]=0;
					m_fhead.type = _nudata.image[idx].image_type;

					bResult = NUC_WritePipe(id, (unsigned char*)&m_fhead, sizeof(NORBOOT_NAND_HEAD));
					if (bResult == FALSE) {
						fprintf(stderr, "%s (%d) - NUC_WritePipe failed %d.\n", __func__, id, __LINE__);
						goto EXIT;
					}
					bResult = NUC_ReadPipe(id, (unsigned char *)&ack, (int)sizeof(unsigned int));
					if (bResult == FALSE) {
						fprintf(stderr, "%s (%d) - NUC_ReadPipe failed %d.\n", __func__, id, __LINE__);
						goto EXIT;
					}
					fread(lpBuffer, 1, m_fhead.filelen, fp);
					break;
				case IMG_T_ENV:
					m_fhead.no = 0;
					m_fhead.execaddr = 0x200;
					m_fhead.flashoffset = _nudata.image[idx].image_start_offset;
					if (file_len > (0x10000-4)) {
						fclose(fp);
						fprintf(stderr, "The environment file size is less then 64KB\n");
						goto EXIT;
					}
					lpBuffer = (unsigned char *)malloc(sizeof(unsigned char)*0x10000); //read file to buffer
					memset(lpBuffer, 0x00, 0x10000);

					m_fhead.macaddr[7] = 0;

					m_fhead.filelen = 0x10000;
					m_fhead.type = _nudata.image[idx].image_type;

					NUC_WritePipe(id, (unsigned char*)&m_fhead, sizeof(NORBOOT_NAND_HEAD));
					if (bResult == FALSE) {
						fprintf(stderr, "%s (%d) - NUC_WritePipe failed %d.\n", __func__, id, __LINE__);
						goto EXIT;
					}
					NUC_ReadPipe(id, (unsigned char *)&ack, (int)sizeof(unsigned int));
					if (bResult == FALSE) {
						fprintf(stderr, "%s (%d) - NUC_ReadPipe failed %d.\n", __func__, id, __LINE__);
						goto EXIT;
					}

					{
						char line[256];
						char* ptr = (char *)(lpBuffer + 4);
						while (1) {
							if (fgets(line,256, fp) == NULL)
								break;
							if ((line[strlen(line) - 2] == 0x0D) &&
								(line[strlen(line) - 1] == 0x0A)) {
								strncpy_s(ptr, 0x20000, line, strlen(line) - 1);
								ptr[strlen(line) - 2] = 0x0;
								ptr += (strlen(line) - 1);
							} else if (line[strlen(line) - 1] == 0x0A) {
								strncpy_s(ptr, 0x20000, line, strlen(line));
								ptr[strlen(line) - 1] = 0x0;
								ptr += (strlen(line));
							} else {
								strncpy_s(ptr, 0x20000, line, strlen(line));
								ptr += strlen(line);
							}
						}
					}
					*(unsigned int *)lpBuffer = CalculateCRC32((unsigned char *)(lpBuffer + 4), 0x10000 - 4);
					file_len = 0x10000;
					break;
					
				case IMG_T_LOADER:
					m_fhead.no = 0;
					m_fhead.execaddr = _nudata.image[idx].image_exe_addr;
					m_fhead.flashoffset = 0;
					ddrbuf = GetDDRFormat(&ddrlen);
					file_len = file_len + ddrlen;
					m_fhead.initSize = ddrlen;

					lpBuffer = (unsigned char *)malloc(sizeof(unsigned char)*file_len);
					memset(lpBuffer, 0xff, file_len);
					m_fhead.macaddr[7] = 0;

					m_fhead.type = _nudata.image[idx].image_type;

					NUC_WritePipe(id, (unsigned char*)&m_fhead, sizeof(NORBOOT_NAND_HEAD));
					if (bResult == FALSE) {
						fprintf(stderr, "%s (%d) - NUC_WritePipe failed %d.\n", __func__, id, __LINE__);
						goto EXIT;
					}
					NUC_ReadPipe(id, (unsigned char *)&ack, (int)sizeof(unsigned int));
					if (bResult == FALSE) {
						fprintf(stderr, "%s (%d) - NUC_ReadPipe failed %d.\n", __func__, id, __LINE__);
						goto EXIT;
					}

					memcpy(lpBuffer, ddrbuf, ddrlen);
					fread(lpBuffer + ddrlen, 1, m_fhead.filelen, fp);
					break;
				}
				fclose(fp);

#if 1
				if (lpBuffer == NULL) {
					fprintf(stderr, "%s %d - lpBuffer is NULL!\n", __func__, __LINE__);
					goto EXIT;
				}

				fprintf(stdout, "Write image%d %s ...\n",idx, TypeT[_nudata.image[idx].image_idx].pName);
				pbuf = lpBuffer;
				scnt = file_len / BUF_SIZE;
				rcnt = file_len % BUF_SIZE;
				total=0;
				while (scnt > 0) {
					bResult = NUC_WritePipe(id, (unsigned char*)pbuf, BUF_SIZE);
					if (bResult == FALSE) {
						fprintf(stderr, "%s (%d) - NUC_WritePipe failed %d.\n", __func__, id, __LINE__);
						goto EXIT;
					}
					pbuf += BUF_SIZE;
					total += BUF_SIZE;
					pos = (int)(((float)(((float)total / (float)file_len)) * 100));
					bResult = NUC_ReadPipe(id, (UCHAR *)&ack, 4);
					if ((bResult == FALSE) || (ack != BUF_SIZE)) {
						fprintf(stderr, "%s (%d) - NUC_ReadPipe failed %d.\n", __func__, id, __LINE__);
						goto EXIT;
					}
					scnt--;
					// show_progressbar(pos);
				}
				if (rcnt > 0) {
					bResult = NUC_WritePipe(id, (unsigned char*)pbuf, rcnt);
					if (bResult == FALSE) {
						fprintf(stderr, "%s (%d) - NUC_WritePipe failed %d.\n", __func__, id, __LINE__);
						goto EXIT;
					}
					total += rcnt;
					bResult = NUC_ReadPipe(id, (UCHAR *)&ack, 4);
					if ((bResult == FALSE) || (ack != rcnt)) {
						fprintf(stderr, "%s (%d) - NUC_ReadPipe failed %d.\n", __func__, id, __LINE__);
						goto EXIT;
					}
				}
				// fprintf(stdout, "Write image%d %s ... ",idx,TypeT[_nudata.image[idx].image_idx].pName);
				// show_progressbar(pos);
				bResult = NUC_ReadPipe(id, (UCHAR *)&blockNum, 4);
				if (bResult == FALSE) {
					fprintf(stderr, "%s (%d) - NUC_ReadPipe failed %d.\n", __func__, id, __LINE__);
					 goto EXIT;
				}
				// show_progressbar(100);
				fprintf(stdout, "Write image%d %s ... Passed\n",idx,TypeT[_nudata.image[idx].image_idx].pName);
				
				if ((_nudata.run == RUN_PROGRAM_VERIFY)) {
					unsigned char temp[BUF_SIZE];

					if (EnableOneWinUsbDevice(id) == FALSE) {
						fprintf(stderr, "%s %d (%d) - Failed to enable WinUsb device!\n", __func__, __LINE__, id);
						return ERR_CODE_USB_CONN;
					}
					NUC_SetType(0, NAND, (UCHAR *)&typeack, sizeof(typeack));
					m_fhead.flag = VERIFY_ACTION;
					NUC_WritePipe(id, (unsigned char*)&m_fhead, sizeof(NORBOOT_NAND_HEAD));
					if (bResult == FALSE) {
						fprintf(stderr, "%s (%d) - NUC_WritePipe failed %d.\n", __func__, id, __LINE__);
						goto EXIT;
					}
					NUC_ReadPipe(id, (unsigned char *)&ack, sizeof(unsigned int));
					if (bResult == FALSE) {
						fprintf(stderr, "%s (%d) - NUC_ReadPipe failed %d.\n", __func__, id, __LINE__);
						goto EXIT;
					}

					fprintf(stdout, "Verify image%d %s ...\n", idx, TypeT[_nudata.image[idx].image_idx].pName);
					pbuf = lpBuffer + m_fhead.initSize;
					scnt=(file_len - m_fhead.initSize) / BUF_SIZE;
					rcnt=(file_len - m_fhead.initSize) % BUF_SIZE;
					total = 0;
					while (scnt > 0) {
						bResult = NUC_ReadPipe(id, (unsigned char*)temp, BUF_SIZE);
						if (bResult >= 0) {
							total += BUF_SIZE;
							if (DataCompare(temp, pbuf, BUF_SIZE))
								ack = BUF_SIZE;
							else
								ack = 0;//compare error
							bResult = NUC_WritePipe(id, (unsigned char*)&ack, 4);
							if (bResult == FALSE) {
								fprintf(stderr, "%s (%d) - NUC_ReadPipe failed %d.\n", __func__, id, __LINE__);
								fprintf(stderr, "Verify image%d %s ... Failed\n",idx,TypeT[_nudata.image[idx].image_idx].pName);
								goto EXIT;
							}
							pos = (int)(((float)(((float)total / (float)file_len)) * 100));
							// show_progressbar(pos);
							pbuf += BUF_SIZE;
						} else {
							fprintf(stderr, "%s (%d) - NUC_ReadPipe failed %d.\n", __func__, id, __LINE__);
							fprintf(stderr, "Verify image%d %s ... Failed\n",idx,TypeT[_nudata.image[idx].image_idx].pName);
							goto EXIT;
						}
						scnt--;
					}

					if (rcnt > 0) {
						bResult = NUC_ReadPipe(id, (UCHAR *)temp, BUF_SIZE);
						if (bResult >= 0) {
							total += rcnt;
							if (DataCompare(temp, pbuf, rcnt))
								ack = BUF_SIZE;
							else
								ack = 0;//compare error
							bResult = NUC_WritePipe(id, (UCHAR *)&ack, 4);
							if ((bResult == FALSE) || (!ack)) {
								fprintf(stderr, "Verify image%d %s ... Failed\n",idx,TypeT[_nudata.image[idx].image_idx].pName);
								goto EXIT;
							}

						} else {
							fprintf(stderr, "Verify image%d %s... Failed\n",idx,TypeT[_nudata.image[idx].image_idx].pName);
							goto EXIT;
						}

						pos = (int)(((float)(((float)total / (float)file_len)) * 100));
						// fprintf(stdout, "Verify %s ... ", TypeT[_nudata.image[idx].image_idx].pName);
						// show_progressbar(pos);
					}
					// show_progressbar(100);
					fprintf(stdout, "Verify image%d %s ... Passed\n",idx,TypeT[_nudata.image[idx].image_idx].pName);
				} //Verify_tag end

				free(lpBuffer);
#endif
			}
		}

		if (_nudata.run == RUN_ERASE) {  //Erase Nand
			int wait_pos = 0;
			unsigned int erase_pos = 0;
			m_fhead.flag = ERASE_ACTION;
			m_fhead.flashoffset = _nudata.erase.start_blocks; //start erase block
			m_fhead.execaddr=_nudata.erase.offset_blocks;  //erase block length

			/* Decide chip erase mode or erase mode                    */
			/* 0: chip erase, 1: erase accord start and length blocks. */
			if (_nudata.erase.offset_blocks == 0xFFFFFFFF)
				m_fhead.type = 0;
			else
				m_fhead.type = 1;

			//m_fhead.no=0xFFFFFFFF;//erase good block
			m_fhead.no = 0xFFFFFFFE;//erase good block and bad block

			bResult = NUC_WritePipe(id, (UCHAR *)&m_fhead, sizeof(NORBOOT_NAND_HEAD));
			if (bResult == FALSE) {
				fprintf(stderr, "%s (%d) - NUC_WritePipe failed %d.\n", __func__, id, __LINE__);
				goto EXIT;
			}
			bResult = NUC_ReadPipe(id, (UCHAR *)&ack, 4);
			if (bResult == FALSE) {
				fprintf(stderr, "%s (%d) - NUC_ReadPipe failed %d.\n", __func__, id, __LINE__);
				goto EXIT;
			}

			erase_pos = 0;
			while (erase_pos != 100) {

				bResult = NUC_ReadPipe(id, (UCHAR *)&ack, 4);
				if (bResult == FALSE) {
					fprintf(stderr, "%s (%d) - NUC_ReadPipe failed %d.\n", __func__, id, __LINE__);
					goto EXIT;
				}
				if ((ack >> 16) & 0xffff) {
					fprintf(stderr, "%s (%d) - ack error %d.\n", __func__, id, __LINE__);
					goto EXIT;
				}
				erase_pos = ack & 0xffff;
				fprintf(stdout, "Erase ... ");
				// show_progressbar(erase_pos);
				if (erase_pos == 95) {
					wait_pos++;
					if (wait_pos > 100) {
						goto EXIT;
					}
				}

			}
			// show_progressbar(100);
			fprintf(stdout, "Erase ... Passed\n");
		}
	}
	return 0;

EXIT:
	return -1;
}


void dump_buffer(unsigned char *buff, int len)
{
	int  i, j;
	
	fprintf(stdout, "\n");
	for (i = 0; i < len; i += 16) {
		for (j = 0; j < 16; j++)
			fprintf(stdout, "%02x ", buff[i+j]);

		fprintf(stdout, "    ");

		for (j = 0; j < 16; j++)
			fprintf(stdout, "%c", buff[i+j]);

		fprintf(stdout, "\n");
	}
	fprintf(stdout, "\n");
}

int UXmodem_SPINOR(int id)
{
	int		idx;
	FILE	*fp;
	int		bResult, pos;
	unsigned int scnt, rcnt, file_len, ack,total, blockcnt;
	unsigned char *pbuf;
	NORBOOT_NAND_HEAD  m_fhead;
	unsigned char *ddrbuf = NULL;
	unsigned int ddrlen;
	char	*lpBuffer = NULL;
	unsigned int burn_pos;
	unsigned int erase_pos = 0;
	int		wait_pos = 0;
	USHORT	typeack;

	if (EnableOneWinUsbDevice(id) == FALSE) {
		fprintf(stderr, "%s %d (%d) - Failed to enable WinUsb device!\n", __func__, __LINE__, id);
		return ERR_CODE_USB_CONN;
	}
	
	// fprintf(stdout, "[%s] (%d)\n", __func__, id);

	NUC_SetType(0, SPI, (UCHAR *)&typeack, sizeof(typeack));

	if (_nudata.image[0].image_type == IMG_T_PACK) {
		if (UXmodem_Pack(id) != 0) {
			fprintf(stderr, "UXmodem_Pack failed!\n");
			goto EXIT;
		} else {
			return 0;
		}
	} else {

		if (_nudata.run == RUN_READ) {
			unsigned char temp[BUF_SIZE];
			
			//-----------------------------------
			if (fopen_s(&fp, _nudata.read.path, "w+b") != 0) {
				fprintf(stderr, "Open write File Error(-w %s) \n", _nudata.read.path);
				goto EXIT;
			}
			file_len = _nudata.read.offset_blocks * (SPI64K); //*m_info.Nand_uPageSize*m_info.Nand_uPagePerBlock;
			m_fhead.flag = READ_ACTION;
			m_fhead.flashoffset = _nudata.read.start_blocks * (SPI64K);
			m_fhead.filelen = file_len;
			m_fhead.initSize = 0; //read good block
			bResult = NUC_WritePipe(id, (UCHAR *)&m_fhead, sizeof(NORBOOT_NAND_HEAD));
			if (bResult == FALSE) {
				fprintf(stderr, "%s (%d) - NUC_WritePipe failed %d.\n", __func__, id, __LINE__);
				goto EXIT;
			}
			bResult = NUC_ReadPipe(id, (unsigned char *)&ack, (int)sizeof(unsigned int));
			if (bResult == FALSE) {
				fprintf(stderr, "%s (%d) - NUC_ReadPipe failed %d.\n", __func__, id, __LINE__);
				goto EXIT;
			}

			scnt = file_len / BUF_SIZE;
			rcnt = file_len % BUF_SIZE;
			total = 0;
			while (scnt > 0) {
				bResult = NUC_ReadPipe(id, (unsigned char*)temp, BUF_SIZE);
				if (bResult == FALSE) {
					fclose(fp);
					fprintf(stderr, "%s (%d) - NUC_ReadPipe failed %d.\n", __func__, id, __LINE__);
					goto EXIT;
				}
				total += BUF_SIZE;
				pos = (int)(((float)(((float)total / (float)file_len)) * 100));
				fprintf(stdout, "Read ... ");
				// show_progressbar(pos);
				fwrite(temp, 1, BUF_SIZE, fp);
				ack = BUF_SIZE;
				bResult = NUC_WritePipe(id, (UCHAR *)&ack, 4);
				if (bResult == FALSE) {
					fclose(fp);
					fprintf(stderr, "%s (%d) - NUC_WritePipe failed %d.\n", __func__, id, __LINE__);
					goto EXIT;
				}
				scnt--;
			}

			if (rcnt > 0) {
				bResult = NUC_ReadPipe(id, (UCHAR *)temp, BUF_SIZE);
				if (bResult == FALSE) {
					fclose(fp);
					fprintf(stderr, "%s (%d) - NUC_ReadPipe failed %d.\n", __func__, id, __LINE__);
					goto EXIT;
				}
				total += rcnt;
				fwrite(temp, 1, rcnt, fp);
				ack = BUF_SIZE;
				bResult = NUC_WritePipe(id, (UCHAR *)&ack, 4);
				if (bResult == FALSE) {
					fclose(fp);
					fprintf(stderr, "%s (%d) - NUC_WritePipe failed %d.\n", __func__, id, __LINE__);
					goto EXIT;
				}
				pos = (int)(((float)(((float)total / (float)file_len)) * 100));
				fprintf(stdout, "Read ... ");
				// show_progressbar(pos);
			}
			fclose(fp);
			// show_progressbar(100);
			fprintf(stdout, "Read ... Passed\n");
		}

		if ((_nudata.run == RUN_PROGRAM) || (_nudata.run == RUN_PROGRAM_VERIFY)) { //Burn Image
			for (idx = 0; idx < _nudata.image_num; idx++) {
				if (idx > 0) {
					if (EnableOneWinUsbDevice(id) == FALSE) {
						fprintf(stderr, "%s %d (%d) - Failed to enable WinUsb device!\n", __func__, __LINE__, id);
						return ERR_CODE_USB_CONN;
					}
					NUC_SetType(0, SPI, (UCHAR *)&typeack, sizeof(typeack));
					usleep(10);
				}
				
				if (fopen_s(&fp, _nudata.image[idx].image_path, "rb") != 0) {
					fprintf(stderr, "Open read File Error(%s) \n", _nudata.image[idx].image_path);
					goto EXIT;
				}

				fseek(fp, 0, SEEK_END);
				file_len = ftell(fp);
				fseek(fp, 0, SEEK_SET);

				if (!file_len) {
					fclose(fp);
					fprintf(stderr, "File length is zero\n");
					goto EXIT;
				}

				NUC_SetType(0, SPI, (UCHAR *)&typeack, sizeof(typeack));
				usleep(10);

				memset((unsigned char *)&m_fhead, 0, sizeof(NORBOOT_NAND_HEAD));
				m_fhead.flag = WRITE_ACTION;
				m_fhead.initSize = 0;
				m_fhead.filelen = file_len;

				fprintf(stdout, "[%s] program file: %s, len = %d\n", __func__, _nudata.image[idx].image_path, file_len);
				
				switch(_nudata.image[idx].image_type) {
				case IMG_T_DATA:
				case IMG_T_PACK:
					m_fhead.no = 0;
					m_fhead.execaddr = _nudata.image[idx].image_exe_addr;
					m_fhead.flashoffset = _nudata.image[idx].image_start_offset;
					lpBuffer = (char *)malloc(file_len); //read file to buffer
					memset(lpBuffer, 0xff, file_len);
					((NORBOOT_NAND_HEAD *)&m_fhead)->macaddr[7] = 0;
					m_fhead.type = _nudata.image[idx].image_type;
					bResult = NUC_WritePipe(id, (unsigned char*)&m_fhead, sizeof(NORBOOT_NAND_HEAD));
					if (bResult == FALSE) {
						fprintf(stderr, "%s (%d) - NUC_WritePipe failed %d.\n", __func__, id, __LINE__);
						goto EXIT;
					}

					bResult = NUC_ReadPipe(id, (unsigned char *)&ack, (int)sizeof(unsigned int));
					if (bResult == FALSE) {
						fprintf(stderr, "%s (%d) - NUC_ReadPipe failed %d.\n", __func__, id, __LINE__);
						goto EXIT;
					}

					if (fread(lpBuffer, 1, file_len, fp) != file_len) {
						fclose(fp);
						fprintf(stderr, "file length is longer\n");
						goto EXIT;
					}
					break;
					
				case IMG_T_ENV:
					m_fhead.no = 0;
					m_fhead.execaddr = 0x200;
					m_fhead.flashoffset = _nudata.image[idx].image_start_offset;
					if(file_len > (0x10000 - 4)) {
						fclose(fp);
						fprintf(stderr, "The environment file size is less then 64KB\n");
						goto EXIT;
					}
					lpBuffer = (char *)malloc(sizeof(unsigned char)*0x10000); //read file to buffer
					memset(lpBuffer, 0x00, 0x10000);

					((NORBOOT_NAND_HEAD *)&m_fhead)->macaddr[7] = 0;

					m_fhead.filelen = 0x10000;
					m_fhead.type = _nudata.image[idx].image_type;

					bResult = NUC_WritePipe(id, (unsigned char*)&m_fhead, sizeof(NORBOOT_NAND_HEAD));
					if (bResult == FALSE) {
						fprintf(stderr, "%s (%d) - NUC_WritePipe failed %d.\n", __func__, id, __LINE__);
						goto EXIT;
					}
					
					bResult = NUC_ReadPipe(id,(unsigned char *)&ack,(int)sizeof(unsigned int));
					if (bResult == FALSE) {
						fprintf(stderr, "%s (%d) - NUC_ReadPipe failed %d.\n", __func__, id, __LINE__);
						goto EXIT;
					}

					{
						char	line[256];
						char	*ptr = (char *)(lpBuffer + 4);
						while (1) {
							if (fgets(line,256, fp) == NULL)
								break;
							
							if ((line[strlen(line)-2] == 0x0D) && (line[strlen(line) - 1] == 0x0A)) {
								strncpy_s(ptr, 0x20000, line,strlen(line) - 1);
								ptr[strlen(line) - 2] = 0x0;
								ptr += (strlen(line) - 1);
							} else if (line[strlen(line) - 1] == 0x0A) {
								strncpy_s(ptr, 0x20000, line, strlen(line));
								ptr[strlen(line) - 1] = 0x0;
								ptr += strlen(line);
							} else {
								strncpy_s(ptr, 0x20000, line, strlen(line));
								ptr += strlen(line);
							}
						}
					}
					*(unsigned int *)lpBuffer = CalculateCRC32((unsigned char *)(lpBuffer+4), 0x10000 - 4);
					file_len = 0x10000;
					break;
					
				case IMG_T_LOADER:
					m_fhead.no = 1;
					m_fhead.execaddr = _nudata.image[idx].image_exe_addr;
					m_fhead.flashoffset = 0;
					ddrbuf = GetDDRFormat(&ddrlen);
					file_len = file_len + ddrlen;
					((NORBOOT_NAND_HEAD *)&m_fhead)->initSize = ddrlen;

					lpBuffer = (char *)malloc(sizeof(unsigned char)*file_len);
					memset(lpBuffer, 0xff, file_len);
					((NORBOOT_NAND_HEAD *)&m_fhead)->macaddr[7] = 0;

					m_fhead.type = _nudata.image[idx].image_type;

					NUC_WritePipe(id, (unsigned char*)&m_fhead, sizeof(NORBOOT_NAND_HEAD));
					if (bResult == FALSE) {
						fprintf(stderr, "%s (%d) - NUC_WritePipe failed %d.\n", __func__, id, __LINE__);
						goto EXIT;
					}
					NUC_ReadPipe(id, (unsigned char *)&ack, (int)sizeof(unsigned int));
					if (bResult == FALSE) {
						fprintf(stderr, "%s (%d) - NUC_ReadPipe failed %d.\n", __func__, id, __LINE__);
						goto EXIT;
					}

					memcpy(lpBuffer, ddrbuf, ddrlen);
					fread(lpBuffer + ddrlen, 1, m_fhead.filelen, fp);
					break;
					
				default:
					fprintf(stderr, "Unknow image type: %d!\n", _nudata.image[idx].image_type);
					bResult = FALSE;
					goto EXIT;
				}
				fclose(fp);

				if (lpBuffer == NULL) {
					fprintf(stderr, "%s %d - lpBuffer is NULL!\n", __func__, __LINE__);
					goto EXIT;
				}
				
				fprintf(stdout, "Write image%d %s ...\n", idx, TypeT[_nudata.image[idx].image_idx].pName);
				pbuf = (unsigned char *)lpBuffer;
				scnt = file_len / BUF_SIZE;
				rcnt = file_len % BUF_SIZE;
				total = 0;
				blockcnt = SPI_BLOCK_SIZE;
				
				while (scnt > 0) {
					bResult = NUC_WritePipe(id, (unsigned char*)pbuf, BUF_SIZE);
					if (bResult == FALSE) {
						fprintf(stderr, "%s (%d) - NUC_WritePipe failed %d.\n", __func__, id, __LINE__);
						goto EXIT;
					}
					pbuf += BUF_SIZE;
					total += BUF_SIZE;
					pos = (int)(((float)(((float)total / (float)file_len)) * 100));
					bResult = NUC_ReadPipe(id, (UCHAR *)&ack, 4);
					if ((bResult == FALSE) || (ack != BUF_SIZE)) {
						goto EXIT;
					}
					scnt--;
					if ((blockcnt == total) && (_nudata.image[idx].image_type != IMG_T_LOADER)) {
						Sleep(1);
						bResult = NUC_ReadPipe(id, (UCHAR *)&ack, 4);
						if (bResult == FALSE) {
							fprintf(stderr, "%s (%d) - NUC_ReadPipe failed %d.\n", __func__, id, __LINE__);
							goto EXIT;
						}
						blockcnt += SPI_BLOCK_SIZE;
					}
					// fprintf(stdout, "Write image%d %s ... ", idx, TypeT[_nudata.image[idx].image_idx].pName);
					// show_progressbar(pos);
				}
				if (rcnt > 0) {
					bResult = NUC_WritePipe(id, (unsigned char*)pbuf, rcnt);
					if (bResult == FALSE) {
						fprintf(stderr, "%s (%d) - NUC_WritePipe failed %d.\n", __func__, id, __LINE__);
						goto EXIT;
					}
					total += rcnt;
					bResult = NUC_ReadPipe(id, (UCHAR *)&ack, 4);
					if ((bResult == FALSE) || (ack != rcnt)) {
						fprintf(stderr, "%s (%d) - NUC_ReadPipe failed %d.\n", __func__, id, __LINE__);
						goto EXIT;
					}
				}
				
				if (_nudata.image[idx].image_type == IMG_T_LOADER) {
					// show_progressbar(pos);
					burn_pos = 0;
					while (burn_pos != 100) {
						bResult = NUC_ReadPipe(id, (UCHAR *)&ack, 4);
						if (bResult == FALSE) {
							fprintf(stderr, "%s (%d) - NUC_ReadPipe failed %d.\n", __func__, id, __LINE__);
							goto EXIT;
						}
						if (!((ack >> 16) & 0xffff)) {
							burn_pos = (UCHAR)(ack & 0xffff);
						} else {
							goto EXIT;
						}
					}
				} else if((file_len % SPI_BLOCK_SIZE) != 0 ){
					bResult = NUC_ReadPipe(id, (UCHAR *)&ack, 4);
					if (bResult == FALSE) {
						fprintf(stderr, "%s (%d) - NUC_ReadPipe failed %d.\n", __func__, id, __LINE__);
						goto EXIT;
					}
				}
				Sleep(1);

				// show_progressbar(100);
				fprintf(stdout, "Write image%d %s ... Passed\n", idx, TypeT[_nudata.image[idx].image_idx].pName);

				if ((_nudata.run == RUN_PROGRAM_VERIFY)) {
					unsigned char temp[BUF_SIZE];

					if (EnableOneWinUsbDevice(id) == FALSE) {
						fprintf(stderr, "%s %d (%d) - Failed to enable WinUsb device!\n", __func__, __LINE__, id);
						return ERR_CODE_USB_CONN;
					}
					
					NUC_SetType(0, SPI, (UCHAR *)&typeack, sizeof(typeack));
					
					m_fhead.flag = VERIFY_ACTION;
					NUC_WritePipe(id, (unsigned char*)&m_fhead, sizeof(NORBOOT_NAND_HEAD));
					if (bResult == FALSE) {
						fprintf(stderr, "%s (%d) - NUC_WritePipe failed %d.\n", __func__, id, __LINE__);
						goto EXIT;
					}
					NUC_ReadPipe(id, (unsigned char *)&ack, sizeof(unsigned int));
					if (bResult == FALSE) {
						fprintf(stderr, "%s (%d) - NUC_ReadPipe failed %d.\n", __func__, id, __LINE__);
						goto EXIT;
					}
					
					fprintf(stdout, "Verify image%d %s...\n", idx, TypeT[_nudata.image[idx].image_idx].pName);
					pbuf = (unsigned char *)lpBuffer + m_fhead.initSize;
					scnt = (file_len - m_fhead.initSize) / BUF_SIZE;
					rcnt = (file_len - m_fhead.initSize) % BUF_SIZE;
					total = 0;
					while (scnt > 0) {
						// fprintf(stdout, "scnt = %d\n", scnt);
						bResult = NUC_ReadPipe(id, (unsigned char*)temp, BUF_SIZE);
						if (bResult == TRUE) {
							total += BUF_SIZE;
							if (DataCompare(temp, pbuf, BUF_SIZE)) {
								ack = BUF_SIZE;
							} else {
								ack = 0; //compare error
								fprintf(stderr, "%s %d - data compare error!\n", __func__, __LINE__);
								dump_buffer(temp, 128);
								dump_buffer(pbuf, 128);
								goto EXIT;
							}
							bResult = NUC_WritePipe(id, (unsigned char*)&ack, 4);
							if (bResult == FALSE) {
								fprintf(stderr,  "Verify image%d %s ... Failed\n", idx, TypeT[_nudata.image[idx].image_idx].pName);
								goto EXIT;
							}
							pos = (int)(((float)(((float)total / (float)file_len)) * 100));
							// fprintf(stdout, "Verify image%d %s ... ", idx, TypeT[_nudata.image[idx].image_idx].pName);
							// show_progressbar(pos);
							pbuf += BUF_SIZE;
						} else {
							fprintf(stderr, "Verify image%d %s ... Failed\n", idx, TypeT[_nudata.image[idx].image_idx].pName);
							goto EXIT;
						}
						scnt--;
					}

					if (rcnt > 0) {
						bResult = NUC_ReadPipe(id, (UCHAR *)temp, rcnt);
						if (bResult == TRUE) {
							total += rcnt;
							if (DataCompare(temp, pbuf, rcnt)) {
								ack = rcnt;
							} else {
								ack = 0; //compare error
								fprintf(stderr, "%s %d - data compare error!\n", __func__, __LINE__);
								dump_buffer(temp, 128);
								dump_buffer(pbuf, 128);
								goto EXIT;
							}

							bResult = NUC_WritePipe(id, (UCHAR *)&ack, 4);
							if ((bResult == FALSE) || (!ack)) {
								fprintf(stderr, "Verify image%d %s ... Failed\n",idx,TypeT[_nudata.image[idx].image_idx].pName);
								goto EXIT;
							}

						} else {
							fprintf(stderr, "Verify image%d %s ... Failed\n",idx,TypeT[_nudata.image[idx].image_idx].pName);
							goto EXIT;
						}

						pos = (int)(((float)(((float)total / (float)file_len)) * 100));
						// fprintf(stdout, "Verify image%d %s ... ", idx, TypeT[_nudata.image[idx].image_idx].pName);
						// show_progressbar(pos);
					}

					Sleep(1);
					// show_progressbar(100);
					fprintf(stdout, "Verify image%d %s... Passed\n", idx, TypeT[_nudata.image[idx].image_idx].pName);
				} //Verify_tag end

				free(lpBuffer);
			}
		}

		if (_nudata.run == RUN_ERASE) {  //Erase SPI
			m_fhead.flag = ERASE_ACTION;
			m_fhead.flashoffset = _nudata.erase.start_blocks; //start erase block
			m_fhead.execaddr = _nudata.erase.offset_blocks;  //erase block length

			/* Decide chip erase mode or erase mode                    */
			/* 0: chip erase, 1: erase accord start and length blocks. */
			if (_nudata.erase.offset_blocks == 0xFFFFFFFF) {
				m_fhead.type = 0;
				m_fhead.no = 0xffffffff;//erase all
			} else {
				m_fhead.type = 1;
				m_fhead.no = 0;
			}

			bResult = NUC_WritePipe(id, (UCHAR *)&m_fhead, sizeof(NORBOOT_NAND_HEAD));
			if (bResult == FALSE) {
				fprintf(stderr, "%s (%d) - NUC_WritePipe failed %d.\n", __func__, id, __LINE__);
				goto EXIT;
			}
			bResult = NUC_ReadPipe(id, (UCHAR *)&ack, 4);
			if (bResult == FALSE) {
				fprintf(stderr, "%s (%d) - NUC_WritePipe failed %d.\n", __func__, id, __LINE__);
				goto EXIT;
			}
			erase_pos = 0;
			wait_pos = 0;
			fprintf(stdout, "[%s] Erase ... \n", __func__);
			// show_progressbar(erase_pos);
			while (erase_pos != 100) {
				bResult = NUC_ReadPipe(id, (UCHAR *)&ack, 4);
				if (bResult == FALSE) {
					fprintf(stderr, "%s (%d) - NUC_ReadPipe failed %d.\n", __func__, id, __LINE__);
					goto EXIT;
				}
				if (((ack >> 16) & 0xffff))
					goto EXIT;
				
				erase_pos = ack & 0xffff;
				fprintf(stdout, "Erase ... ");
				// show_progressbar(erase_pos);
				if (erase_pos == 95) {
					wait_pos++;
					if (wait_pos > 100) {
						goto EXIT;
					}
				}
			}
			// show_progressbar(100);
			fprintf(stdout, "[%s] Erase ... Passed\n", __func__);
		}
	}
	
	fprintf(stdout, "[%s] done\n", __func__);
	return 0;

EXIT:

	fprintf(stderr, "[%s] failed!\n", __func__);
	return -1;
}

int UXmodem_SPINAND(int id)
{
	int		idx;
	FILE	*fp;
	int		bResult, pos;
	unsigned int scnt, rcnt, file_len, ack, total;
	unsigned char *pbuf;
	NORBOOT_NAND_HEAD  m_fhead;
	unsigned char *ddrbuf = NULL;
	unsigned int ddrlen;
	unsigned char  *lpBuffer = NULL;
	int		blockNum;
	USHORT	typeack;
	
	if (EnableOneWinUsbDevice(id) == FALSE) {
		fprintf(stderr, "%s %d (%d) - Failed to enable WinUsb device!\n", __func__, __LINE__, id);
		return ERR_CODE_USB_CONN;
	}

	NUC_SetType(0, SPINAND, (UCHAR *)&typeack, sizeof(typeack));

	if (_nudata.image[0].image_type == IMG_T_PACK) {
		if (UXmodem_Pack(id) != 0) {
			fprintf(stderr, "UXmodem_Pack failed!\n");
			goto EXIT;
		} else {
			return 0;
		}
	} else {

		if (_nudata.run == RUN_READ) {
			unsigned char temp[BUF_SIZE];
			
			//-----------------------------------
			if (fopen_s(&fp, _nudata.read.path, "w+b") != 0) {
				fprintf(stderr, "Open write File Error(-w %s) \n", _nudata.read.path);
				goto EXIT;
			}
			file_len = _nudata.read.offset_blocks * (_nudata.user_def.SPINand_PagePerBlock * _nudata.user_def.SPINand_PageSize);
			m_fhead.flag = READ_ACTION;
			m_fhead.flashoffset = _nudata.read.start_blocks * (_nudata.user_def.SPINand_PagePerBlock * _nudata.user_def.SPINand_PageSize);
			m_fhead.filelen = file_len;
			m_fhead.initSize = 0; //read good block
			bResult = NUC_WritePipe(id, (UCHAR *)&m_fhead, sizeof(NORBOOT_NAND_HEAD));
			if (bResult == FALSE) {
				fprintf(stderr, "%s (%d) - NUC_WritePipe failed %d.\n", __func__, id, __LINE__);
				goto EXIT;
			}
			bResult = NUC_ReadPipe(id, (unsigned char *)&ack, (int)sizeof(unsigned int));
			if (bResult == FALSE) {
				fprintf(stderr, "%s (%d) - NUC_ReadPipe failed %d.\n", __func__, id, __LINE__);
				goto EXIT;
			}

			scnt = file_len / BUF_SIZE;
			rcnt = file_len % BUF_SIZE;
			total = 0;
			while (scnt > 0) {
				bResult = NUC_ReadPipe(id, (unsigned char*)temp, BUF_SIZE);
				if (bResult == FALSE) {
					fclose(fp);
					goto EXIT;
				}
				total += BUF_SIZE;
				pos = (int)(((float)(((float)total / (float)file_len)) * 100));
				fprintf(stdout, "Read ... ");
				// show_progressbar(pos);
				fwrite(temp, 1, BUF_SIZE, fp);
				ack = BUF_SIZE;
				bResult = NUC_WritePipe(id, (UCHAR *)&ack, 4);
				if (bResult == FALSE) {
					fclose(fp);
					fprintf(stderr, "%s (%d) - NUC_WritePipe failed %d.\n", __func__, id, __LINE__);
					goto EXIT;
				}
				scnt--;
			}

			if (rcnt > 0) {
				bResult = NUC_ReadPipe(id, (UCHAR *)temp, BUF_SIZE);
				if (bResult == FALSE) {
					fclose(fp);
					fprintf(stderr, "%s (%d) - NUC_ReadPipe failed %d.\n", __func__, id, __LINE__);
					goto EXIT;
				}
				total += rcnt;
				fwrite(temp, 1, rcnt, fp);
				ack = BUF_SIZE;
				bResult = NUC_WritePipe(id, (UCHAR *)&ack, 4);
				if (bResult == FALSE) {
					fclose(fp);
					fprintf(stderr, "%s (%d) - NUC_WritePipe failed %d.\n", __func__, id, __LINE__);
					goto EXIT;
				}
				pos = (int)(((float)(((float)total / (float)file_len)) * 100));
				fprintf(stdout, "Read ... ");
				// show_progressbar(pos);
			}
			fclose(fp);
			// show_progressbar(100);
			fprintf(stdout, "Read ... Passed\n");
		}

		if ((_nudata.run == RUN_PROGRAM) || (_nudata.run == RUN_PROGRAM_VERIFY)) { //Burn Image
			for (idx = 0; idx < _nudata.image_num; idx++) {
				if (idx > 0) {
					if (EnableOneWinUsbDevice(id) == FALSE) {
						fprintf(stderr, "%s %d (%d) - Failed to enable WinUsb device!\n", __func__, __LINE__, id);
						return ERR_CODE_USB_CONN;
					}
					NUC_SetType(0, SPINAND, (UCHAR *)&typeack, sizeof(typeack));
					usleep(10);
				}
				if (fopen_s(&fp, _nudata.image[idx].image_path, "rb") != 0) {
					fprintf(stderr, "Open read File Error(%s) \n", _nudata.image[idx].image_path);
					goto EXIT;
				}
				fprintf(stdout, "image%d %s\n",idx,_nudata.image[idx].image_path);

				fseek(fp, 0, SEEK_END);
				file_len = ftell(fp);
				fseek(fp, 0, SEEK_SET);

				if (!file_len) {
					fclose(fp);
					fprintf(stderr, "File length is zero\n");
					goto EXIT;
				}
				memset((unsigned char *)&m_fhead, 0, sizeof(NORBOOT_NAND_HEAD));
				m_fhead.flag = WRITE_ACTION;
				m_fhead.initSize = 0;
				m_fhead.filelen = file_len;
				
				switch (_nudata.image[idx].image_type) {
				case IMG_T_DATA:
				case IMG_T_PACK:
					m_fhead.no = 0;
					m_fhead.execaddr = 0x200;
					m_fhead.flashoffset = _nudata.image[idx].image_start_offset;
					lpBuffer = (unsigned char *)malloc(sizeof(unsigned char) * file_len); //read file to buffer
					memset(lpBuffer, 0xff, file_len);
					((NORBOOT_NAND_HEAD *)&m_fhead)->macaddr[7] = 0;
					m_fhead.type = _nudata.image[idx].image_type;
					bResult = NUC_WritePipe(id, (unsigned char*)&m_fhead, sizeof(NORBOOT_NAND_HEAD));
					if (bResult == FALSE) {
						fclose(fp);
						fprintf(stderr, "%s (%d) - NUC_WritePipe failed %d.\n", __func__, id, __LINE__);
						goto EXIT;
					}
					bResult=NUC_ReadPipe(id,(unsigned char *)&ack,(int)sizeof(unsigned int));
					if (bResult == FALSE) {
						fclose(fp);
						fprintf(stderr, "%s (%d) - NUC_ReadPipe failed %d.\n", __func__, id, __LINE__);
						goto EXIT;
					}
					if (fread(lpBuffer, 1, m_fhead.filelen, fp) != m_fhead.filelen) {
						fclose(fp);
						fprintf(stderr, "fjile length is longer\n");
						goto EXIT;
					}
					break;
					
				case IMG_T_ENV:
					m_fhead.no = 0;
					m_fhead.execaddr = 0x200;
					m_fhead.flashoffset = _nudata.image[idx].image_start_offset;
					if (file_len > (SPINAND_ENV_LEN - 4)) {
						fprintf(stderr, "The environment file size is less then 64KB\n");
						goto EXIT;
					}
					lpBuffer = (unsigned char *)malloc(sizeof(unsigned char) * SPINAND_ENV_LEN); //read file to buffer
					memset(lpBuffer, 0x00, SPINAND_ENV_LEN);

					((NORBOOT_NAND_HEAD *)&m_fhead)->macaddr[7] = 0;

					m_fhead.filelen = SPINAND_ENV_LEN;
					m_fhead.type = _nudata.image[idx].image_type;

					NUC_WritePipe(id, (unsigned char*)&m_fhead, sizeof(NORBOOT_NAND_HEAD));
					if (bResult == FALSE) {
						fprintf(stderr, "%s (%d) - NUC_WritePipe failed %d.\n", __func__, id, __LINE__);
						goto EXIT;
					}
					NUC_ReadPipe(id, (unsigned char *)&ack, (int)sizeof(unsigned int));
					if (bResult == FALSE) {
						fprintf(stderr, "%s (%d) - NUC_ReadPipe failed %d.\n", __func__, id, __LINE__);
						goto EXIT;
					}

					{
						char line[256];
						char* ptr = (char *)(lpBuffer + 4);
						while (1) {
							if (fgets(line, 256, fp) == NULL)
								break;
							if ((line[strlen(line)-2] == 0x0D) && (line[strlen(line)-1] == 0x0A)) {
								strncpy_s(ptr, 0x20000, line, strlen(line) - 1);
								ptr[strlen(line)-2] = 0x0;
								ptr += (strlen(line) - 1);
							} else if (line[strlen(line)-1] == 0x0A) {
								strncpy_s(ptr, 0x20000, line, strlen(line));
								ptr[strlen(line)-1] = 0x0;
								ptr += strlen(line);
							} else {
								strncpy_s(ptr, 0x20000, line, strlen(line));
								ptr += strlen(line);
							}
						}

					}
					*(unsigned int *)lpBuffer = CalculateCRC32((unsigned char *)(lpBuffer + 4), SPINAND_ENV_LEN - 4);
					file_len = SPINAND_ENV_LEN;
					break;
					
				case IMG_T_LOADER:
					m_fhead.no=0;
					m_fhead.execaddr = _nudata.image[idx].image_exe_addr;
					m_fhead.flashoffset = 0;
					ddrbuf = GetDDRFormat(&ddrlen);
					file_len = file_len+ddrlen;
					((NORBOOT_NAND_HEAD *)&m_fhead)->initSize = ddrlen;

					lpBuffer = (unsigned char *)malloc(sizeof(unsigned char) * file_len);
					memset(lpBuffer, 0xff, file_len);
					((NORBOOT_NAND_HEAD *)&m_fhead)->macaddr[7] = 0;

					m_fhead.type = _nudata.image[idx].image_type;

					bResult = NUC_WritePipe(id, (unsigned char*)&m_fhead, sizeof(NORBOOT_NAND_HEAD));
					if (bResult == FALSE) {
						fprintf(stderr, "%s (%d) - NUC_WritePipe failed %d.\n", __func__, id, __LINE__);
						goto EXIT;
					}
					bResult = NUC_ReadPipe(id, (unsigned char *)&ack, (int)sizeof(unsigned int));
					if (bResult == FALSE) {
						fprintf(stderr, "%s (%d) - NUC_ReadPipe failed %d.\n", __func__, id, __LINE__);
						goto EXIT;
					}

					memcpy(lpBuffer, ddrbuf, ddrlen);
					fread(lpBuffer + ddrlen, 1, m_fhead.filelen, fp);
					break;
				}
				fclose(fp);
#if 1
				if (lpBuffer == NULL) {
					fprintf(stderr, "%s %d - lpBuffer is NULL!\n", __func__, __LINE__);
					goto EXIT;
				}
				pbuf = lpBuffer;
				scnt = file_len / BUF_SIZE;
				rcnt = file_len % BUF_SIZE;
				total = 0;
				while (scnt > 0) {
					bResult = NUC_WritePipe(id, (unsigned char*)pbuf, BUF_SIZE);
					if (bResult == FALSE) {
						fprintf(stderr, "%s (%d) - NUC_WritePipe failed %d.\n", __func__, id, __LINE__);
						goto EXIT;
					}
					pbuf += BUF_SIZE;
					total += BUF_SIZE;
					pos = (int)(((float)(((float)total / (float)file_len)) * 100));
					bResult = NUC_ReadPipe(id, (UCHAR *)&ack, 4);
					if ((bResult == FALSE) || (ack != BUF_SIZE)) {
						fprintf(stderr, "%s (%d) - NUC_ReadPipe failed %d.\n", __func__, id, __LINE__);
						goto EXIT;
					}
					scnt--;
					// fprintf(stdout, "Write image%d %s ... ", idx, TypeT[_nudata.image[idx].image_idx].pName);
					// show_progressbar(pos);
				}
				if (rcnt > 0) {
					bResult = NUC_WritePipe(id, (unsigned char*)pbuf, rcnt);
					if (bResult == FALSE) {
						fprintf(stderr, "%s (%d) - NUC_WritePipe failed %d.\n", __func__, id, __LINE__);
						goto EXIT;
					}
					total += rcnt;
					bResult = NUC_ReadPipe(id, (UCHAR *)&ack, 4);
					if ((bResult == FALSE) || (ack != rcnt)) {
						fprintf(stderr, "%s (%d) - NUC_ReadPipe failed %d.\n", __func__, id, __LINE__);
						goto EXIT;
					}
				}
				// fprintf(stdout, "Write image%d %s ... ", idx, TypeT[_nudata.image[idx].image_idx].pName);
				// show_progressbar(pos);
				
				bResult = NUC_ReadPipe(id, (UCHAR *)&blockNum, 4);
				if (bResult == FALSE) {
						fprintf(stderr, "%s (%d) - NUC_ReadPipe failed %d.\n", __func__, id, __LINE__);
						goto EXIT;
				}
				
				// show_progressbar(100);
				fprintf(stdout, "Write image%d %s ... Passed\n", idx, TypeT[_nudata.image[idx].image_idx].pName);

				if ((_nudata.run == RUN_PROGRAM_VERIFY)) {
					unsigned char temp[BUF_SIZE];

					if (EnableOneWinUsbDevice(id) == FALSE) {
						fprintf(stderr, "%s %d (%d) - Failed to enable WinUsb device!\n", __func__, __LINE__, id);
						return ERR_CODE_USB_CONN;
					}

					NUC_SetType(0, SPINAND, (UCHAR *)&typeack, sizeof(typeack));

					m_fhead.flag = VERIFY_ACTION;
					bResult = NUC_WritePipe(id, (unsigned char*)&m_fhead, sizeof(NORBOOT_NAND_HEAD));
					if (bResult == FALSE) {
						fprintf(stderr, "%s (%d) - NUC_WritePipe failed %d.\n", __func__, id, __LINE__);
						goto EXIT;
					}
					bResult = NUC_ReadPipe(id, (unsigned char *)&ack, sizeof(unsigned int));
					if (bResult == FALSE) {
						fprintf(stderr, "%s (%d) - NUC_ReadPipe failed %d.\n", __func__, id, __LINE__);
						goto EXIT;
					}

					pbuf = lpBuffer + m_fhead.initSize;
					scnt=(file_len - m_fhead.initSize) / BUF_SIZE;
					rcnt=(file_len - m_fhead.initSize) % BUF_SIZE;
					total = 0;
					while (scnt > 0) {
						bResult = NUC_ReadPipe(id, (unsigned char*)temp, BUF_SIZE);
						if (bResult == FALSE) {
							fprintf(stderr, "%s (%d) %d - Verify image%d %s ... Failed\n",
										__func__, id, __LINE__, idx, TypeT[_nudata.image[idx].image_idx].pName);
							goto EXIT;
						}

						total += BUF_SIZE;
						if (DataCompare(temp, pbuf, BUF_SIZE))
							ack = BUF_SIZE;
						else
							ack = 0;  //compare error
						
						bResult = NUC_WritePipe(id, (unsigned char*)&ack, 4);
						if (bResult == FALSE) {
							fprintf(stderr, "%s (%d) %d - Verify image%d %s ... Failed\n",
										__func__, id, __LINE__, idx, TypeT[_nudata.image[idx].image_idx].pName);
							goto EXIT;
						}
						pos = (int)(((float)(((float)total / (float)file_len)) * 100));
						// fprintf(stdout, "Verify image%d %s ... ", idx, TypeT[_nudata.image[idx].image_idx].pName);
						// show_progressbar(pos);
						pbuf += BUF_SIZE;

						scnt--;
					}

					if (rcnt > 0) {
						bResult = NUC_ReadPipe(id,(UCHAR *)temp,BUF_SIZE);
						if (bResult == FALSE) {
							fprintf(stderr, "%s (%d) %d - Verify image%d %s ... Failed\n",
										__func__, id, __LINE__, idx, TypeT[_nudata.image[idx].image_idx].pName);
							goto EXIT;
						}
						total += rcnt;
						if (DataCompare(temp, pbuf, rcnt))
							ack = BUF_SIZE;
						else
							ack = 0;  // compare error
						
						bResult = NUC_WritePipe(id, (UCHAR *)&ack, 4);
						if ((bResult == FALSE) || (!ack)) {
							fprintf(stderr, "%s (%d) %d - Verify image%d %s ... Failed\n",
										__func__, id, __LINE__, idx,TypeT[_nudata.image[idx].image_idx].pName);
							goto EXIT;
						}

						pos = (int)(((float)(((float)total / (float)file_len)) * 100));
						// fprintf(stdout, "Verify image%d %s ... ", idx, TypeT[_nudata.image[idx].image_idx].pName);
						// show_progressbar(pos);
					}
					// show_progressbar(100);
					fprintf(stdout, "Verify image%d %s... Passed\n",idx,TypeT[_nudata.image[idx].image_idx].pName);
				} //Verify_tag end

				free(lpBuffer);
#endif
			}
		}

		if (_nudata.run == RUN_ERASE) {  //Erase SPI
			int		wait_pos = 0;
			unsigned int erase_pos = 0, BlockPerFlash;
			
			m_fhead.flag = ERASE_ACTION;
			m_fhead.flashoffset = _nudata.erase.start_blocks; //start erase block
			m_fhead.execaddr = _nudata.erase.offset_blocks;  //erase block length

			/* Decide chip erase mode or erase mode                    */
			/* 0: chip erase, 1: erase accord start and length blocks. */
			if (_nudata.erase.offset_blocks == 0xFFFFFFFF) {
				m_fhead.type = 0;
				m_fhead.no = 0xffffffff;//erase all
			} else {
				m_fhead.type = 1;
				//m_fhead.no = 0;
				m_fhead.no = 0xFFFFFFFF;  // 2024.01.22
			}

			bResult = NUC_WritePipe(id, (UCHAR *)&m_fhead, sizeof(NORBOOT_NAND_HEAD));
			if (bResult == FALSE) {
				fprintf(stderr, "%s (%d) - NUC_WritePipe failed %d.\n", __func__, id, __LINE__);
				goto EXIT;
			}
			bResult = NUC_ReadPipe(id, (UCHAR *)&ack, 4);
			if (bResult == FALSE) {
				fprintf(stderr, "SPI NAND Erase error\n");
				fprintf(stderr, "%s (%d) - NUC_ReadPipe failed %d.\n", __func__, id, __LINE__);
				goto EXIT;
			}
			bResult = NUC_ReadPipe(id, (UCHAR *)&BlockPerFlash, 4);
			if (bResult == FALSE) {
				fprintf(stderr, "SPI NAND  BlockPerFlash error !\n");
				goto EXIT;
			}
			erase_pos = 0;
			fprintf(stdout, "Erase ==>\n");
			// show_progressbar(erase_pos);
			while (erase_pos != 100) {
				usleep(1);
				bResult = NUC_ReadPipe(id,(UCHAR *)&ack,4);
				if (bResult == FALSE) {
					fprintf(stderr, "%s (%d) - NUC_ReadPipe failed %d.\n", __func__, id, __LINE__);
					goto EXIT;
				}
				erase_pos = ack;
				fprintf(stderr, "%d%c erased     \r", erase_pos, '%');
#if 0
				// fprintf(stderr, "ack=%d, erase_pos=%d, BlockPerFlash=%d !\n", ack, erase_pos, BlockPerFlash);
				if (ack < BlockPerFlash)
				{
					erase_pos = (int)(((float)(((float)ack / (float)BlockPerFlash)) * 100));
					if (ack == BlockPerFlash - 1) {
						if (erase_pos < 99)
							erase_pos = 100;
						else
							erase_pos++;
					}

				} else {
					fprintf(stderr, "erase_pos=%d, BlockPerFlash=%d !\n", erase_pos, BlockPerFlash);
					fprintf(stderr, "SPI NAND Erase error. ack=%d !\n",ack);
					goto EXIT;
				}
#endif
			}
			// show_progressbar(100);
			fprintf(stdout, "\nErase ... Passed\n");
		}
	}
	return 0;

EXIT:

	return -1;
}

int UXmodem_SD(int id)
{
	int		idx;
	FILE	*fp;
	int		bResult, pos;
	unsigned int scnt, rcnt, file_len, ack, total;
	unsigned char *pbuf;
	NORBOOT_MMC_HEAD  m_fhead;
	unsigned char *ddrbuf = NULL;
	unsigned int ddrlen;
	char	*lpBuffer = NULL;
	int		blockNum;
	USHORT	typeack;

	m_fhead.ReserveSize = _mmc_head.ReserveSize;
	m_fhead.flashoffset = 0x400;
	m_fhead.PartitionNum = _mmc_head.PartitionNum;
	m_fhead.Partition1Size = _mmc_head.Partition1Size;
	m_fhead.Partition2Size = _mmc_head.Partition2Size;
	m_fhead.Partition3Size = _mmc_head.Partition3Size;
	m_fhead.Partition4Size = _mmc_head.Partition4Size;

	if (m_fhead.PartitionNum > 4) {
		fprintf(stderr, "%s %d - PartitionNum %d, it should be 1 ~ 4\n", __func__, id, m_fhead.PartitionNum);
		goto EXIT;
	}

	if (EnableOneWinUsbDevice(id) == FALSE) {
		fprintf(stderr, "%s %d (%d) - Failed to enable WinUsb device!\n", __func__, __LINE__, id);
		return ERR_CODE_USB_CONN;
	}

	NUC_SetType(0, MMC, (UCHAR *)&typeack, sizeof(typeack));
	
	if (_nudata.image[0].image_type == IMG_T_PACK) {
		if (UXmodem_Pack(id) != 0) {
			fprintf(stderr, "UXmodem_Pack failed!\n");
			goto EXIT;
		} else {
			return 0;
		}
	} else {
		if (_nudata.run == RUN_FORMAT) {
			int		wait_pos = 0;
			unsigned int format_pos = 0;
			
			m_fhead.flag = FORMAT_ACTION;
			if (m_fhead.PartitionNum == 1) {
				m_fhead.Partition1Size = _nudata.user_def.EMMC_uBlock / (2 * 1024) -
											_nudata.user_def.EMMC_uReserved;
			} else if(m_fhead.PartitionNum == 2) {
				m_fhead.Partition2Size = _nudata.user_def.EMMC_uBlock / (2 * 1024) -
											_nudata.user_def.EMMC_uReserved -
											m_fhead.Partition1Size ;
			} else if (m_fhead.PartitionNum == 3) {
				m_fhead.Partition3Size = _nudata.user_def.EMMC_uBlock / (2 * 1024) -
											_nudata.user_def.EMMC_uReserved -
											m_fhead.Partition1Size - m_fhead.Partition2Size;
			} else {
				m_fhead.Partition4Size = _nudata.user_def.EMMC_uBlock / (2 * 1024) -
											_nudata.user_def.EMMC_uReserved - m_fhead.Partition1Size -
											m_fhead.Partition2Size - m_fhead.Partition3Size;
			}
			
			fprintf(stdout, "num=%d p1=%d, p2=%d, p3=%d\n", m_fhead.PartitionNum, m_fhead.Partition1Size,
										m_fhead.Partition2Size, m_fhead.Partition3Size);
			m_fhead.ReserveSize = m_fhead.ReserveSize * (2 * 1024);
			bResult = NUC_WritePipe(id, (UCHAR *)&m_fhead, sizeof(NORBOOT_MMC_HEAD));
			if (bResult == FALSE) {
				fprintf(stderr, "%s (%d) - NUC_WritePipe failed %d.\n", __func__, id, __LINE__);
				goto EXIT;
			}
			bResult = NUC_ReadPipe(id, (unsigned char *)&ack, (int)sizeof(unsigned int));
			if (bResult == FALSE) {
				fprintf(stderr, "%s (%d) - NUC_ReadPipe failed %d.\n", __func__, id, __LINE__);
				goto EXIT;
			}
			
			format_pos = 0;
			fprintf(stdout, "Format ... ");
			// show_progressbar(format_pos);
			while (format_pos != 100) {
				bResult = NUC_ReadPipe(id, (UCHAR *)&ack, 4);
				if (bResult == FALSE) {
					fprintf(stderr, "%s (%d) - NUC_ReadPipe failed %d.\n", __func__, id, __LINE__);
					goto EXIT;
				}
				if (((ack >> 16) & 0xffff))
					goto EXIT;
				
				format_pos = ack & 0xffff;
				fprintf(stdout, "Format ... ");
				// show_progressbar(format_pos);
				if (format_pos == 95) {
					wait_pos++;
					if(wait_pos > 100) {
						goto EXIT;
					}
				}
			}
			// show_progressbar(100);
			fprintf(stdout, "Format ... Passed\n");
		}

		if (_nudata.run == RUN_READ) {
			unsigned char temp[BUF_SIZE];

			//-----------------------------------
			if (fopen_s(&fp, _nudata.read.path, "w+b") != 0) {
				fprintf(stderr, "Open write File Error(-w %s) \n",_nudata.read.path);
				goto EXIT;
			}
			file_len = _nudata.read.offset_blocks * (MMC512B);//*m_info.Nand_uPageSize*m_info.Nand_uPagePerBlock;
			m_fhead.flag = READ_ACTION;
			m_fhead.flashoffset = _nudata.read.start_blocks * (MMC512B);
			m_fhead.filelen = file_len;
			m_fhead.initSize = 0;	//read good block
			bResult = NUC_WritePipe(id, (UCHAR *)&m_fhead, sizeof(NORBOOT_MMC_HEAD));
			if (bResult == FALSE) {
				fprintf(stderr, "%s (%d) - NUC_WritePipe failed %d.\n", __func__, id, __LINE__);
				goto EXIT;
			}
			bResult = NUC_ReadPipe(id, (unsigned char *)&ack, (int)sizeof(unsigned int));
			if (bResult == FALSE) {
				fprintf(stderr, "%s (%d) - NUC_ReadPipe failed %d.\n", __func__, id, __LINE__);
				goto EXIT;
			}

			scnt = file_len / BUF_SIZE;
			rcnt = file_len % BUF_SIZE;
			total = 0;
			while (scnt > 0) {
				bResult = NUC_ReadPipe(id,(unsigned char*)temp, BUF_SIZE);
				if (bResult == FALSE) {
					fclose(fp);
					goto EXIT;
				}
				total += BUF_SIZE;
				pos = (int)(((float)(((float)total / (float)file_len)) * 100));
				// fprintf(stdout, "Read ...\n ");
				// show_progressbar(pos);
				fwrite(temp, BUF_SIZE, 1, fp);
				ack = BUF_SIZE;
				//fprintf(stdout, "ACK %d\n", scnt);
				Sleep(20);
				bResult = NUC_WritePipe(id, (UCHAR *)&ack, 4);
				if (bResult == FALSE) {
					fprintf(stderr, "%s (%d) - NUC_WritePipe failed %d.\n", __func__, id, __LINE__);
					fclose(fp);
					goto EXIT;
				}
				scnt--;
			}

			if (rcnt > 0) {
				bResult = NUC_ReadPipe(id, (UCHAR *)temp, BUF_SIZE);
				if (bResult == FALSE) {
					fprintf(stderr, "%s (%d) - NUC_ReadPipe failed %d.\n", __func__, id, __LINE__);
					fclose(fp);
					goto EXIT;
				}
				total += rcnt;
				fwrite(temp, rcnt, 1, fp);
				ack = BUF_SIZE;
				bResult = NUC_WritePipe(id, (UCHAR *)&ack, 4);
				if (bResult == FALSE) {
					fclose(fp);
					fprintf(stderr, "%s (%d) - NUC_WritePipe failed %d.\n", __func__, id, __LINE__);
					goto EXIT;
				}
				pos = (int)(((float)(((float)total / (float)file_len)) * 100));
				fprintf(stdout, "Read ... ");
				// show_progressbar(pos);
			}
			fclose(fp);
			// show_progressbar(100);
			fprintf(stdout, "Read ... Passed\n");
		}

		if ((_nudata.run == RUN_PROGRAM) || (_nudata.run == RUN_PROGRAM_VERIFY)) { //Burn Image
			for (idx = 0; idx < _nudata.image_num; idx++) {
				if (idx > 0) {
					if (EnableOneWinUsbDevice(id) == FALSE) {
						fprintf(stderr, "%s %d (%d) - Failed to enable WinUsb device!\n", __func__, __LINE__, id);
						return ERR_CODE_USB_CONN;
					}
					NUC_SetType(0, MMC, (UCHAR *)&typeack, sizeof(typeack));
					usleep(10);
				}

				if (fopen_s(&fp, _nudata.image[idx].image_path, "rb") != 0) {
					fprintf(stderr, "%s %d - Open read File Error(-w %s) \n", __func__, __LINE__, _nudata.image[idx].image_path);
					goto EXIT;
				}
				fprintf(stdout, "image%d %s\n", idx, _nudata.image[idx].image_path);
				fseek(fp, 0, SEEK_END);
				file_len = ftell(fp);
				fseek(fp, 0, SEEK_SET);

				if (!file_len) {
					fclose(fp);
					fprintf(stderr, "File length is zero\n");
					goto EXIT;
				}
				memset((unsigned char *)&m_fhead, 0, sizeof(NORBOOT_MMC_HEAD));
				m_fhead.flag = WRITE_ACTION;
				m_fhead.initSize = 0;
				m_fhead.filelen = file_len;
				
				switch(_nudata.image[idx].image_type) {
				case IMG_T_DATA:
				case IMG_T_PACK:
					m_fhead.no = 0;
					m_fhead.execaddr = 0x200;
					m_fhead.flashoffset = _nudata.image[idx].image_start_offset;
					lpBuffer = (char *)malloc(sizeof(unsigned char) * file_len); //read file to buffer
					memset(lpBuffer, 0xff, file_len);
					((NORBOOT_MMC_HEAD *)&m_fhead)->macaddr[7] = 0;
					m_fhead.type = _nudata.image[idx].image_type;

					bResult = NUC_WritePipe(id, (unsigned char*)&m_fhead, sizeof(NORBOOT_MMC_HEAD));
					if (bResult == FALSE) {
						fprintf(stderr, "%s (%d) - NUC_WritePipe failed %d.\n", __func__, id, __LINE__);
						goto EXIT;
					}
					bResult=NUC_ReadPipe(id, (unsigned char *)&ack, (int)sizeof(unsigned int));
					if (bResult == FALSE) {
						fprintf(stderr, "%s (%d) - NUC_ReadPipe failed %d.\n", __func__, id, __LINE__);
						goto EXIT;
					}
					fread(lpBuffer, 1, m_fhead.filelen, fp);
					break;

				case IMG_T_ENV:
					m_fhead.no=0;
					m_fhead.execaddr = 0x200;
					m_fhead.flashoffset = _nudata.image[idx].image_start_offset;
					if (file_len > (0x10000 -4)) {
						fclose(fp);
						fprintf(stderr, "The environment file size is less then 64KB\n");
						goto EXIT;
					}
					lpBuffer = (char *)malloc(sizeof(unsigned char)*0x10000); //read file to buffer
					memset(lpBuffer, 0x00, 0x10000);

					((NORBOOT_NAND_HEAD *)&m_fhead)->macaddr[7] = 0;

					m_fhead.filelen = 0x10000;
					m_fhead.type = _nudata.image[idx].image_type;
					bResult = NUC_WritePipe(id, (unsigned char*)&m_fhead, sizeof(NORBOOT_MMC_HEAD));
					if (bResult == FALSE) {
						fprintf(stderr, "%s (%d) - NUC_WritePipe failed %d.\n", __func__, id, __LINE__);
						goto EXIT;
					}
					bResult = NUC_ReadPipe(id,(unsigned char *)&ack,(int)sizeof(unsigned int));
					if (bResult == FALSE) {
						fprintf(stderr, "%s (%d) - NUC_ReadPipe failed %d.\n", __func__, id, __LINE__);
						goto EXIT;
					}

					{
						char	line[256];
						char	*ptr = (char *)(lpBuffer + 4);
						
						while (1) {
							if (fgets(line, 256, fp) == NULL)
								break;
							if ((line[strlen(line)-2] == 0x0D) && (line[strlen(line)-1] == 0x0A)) {
								strncpy_s(ptr, 0x20000, line, strlen(line) - 1);
								ptr[strlen(line) - 2] = 0x0;
								ptr += (strlen(line) - 1);
							} else if (line[strlen(line) - 1]==0x0A) {
								strncpy_s(ptr, 0x20000, line, strlen(line));
								ptr[strlen(line) - 1] = 0x0;
								ptr += strlen(line);
							} else {
								strncpy_s(ptr, 0x20000, line, strlen(line));
								ptr += strlen(line);
							}
						}

					}
					*(unsigned int *)lpBuffer = CalculateCRC32((unsigned char *)(lpBuffer + 4), 0x10000 - 4);
					file_len = 0x10000;
					break;
					
				case IMG_T_LOADER:
					m_fhead.no = 1;
					m_fhead.execaddr = _nudata.image[idx].image_exe_addr;
					m_fhead.flashoffset = 0x400;
					strcpy_s(m_fhead.name, sizeof(m_fhead.name), "u-boot");
					ddrbuf = GetDDRFormat(&ddrlen);
					file_len = file_len + ddrlen + 32;
					m_fhead.initSize = ddrlen;
					lpBuffer = (char *)malloc(sizeof(unsigned char)*file_len);
					memset(lpBuffer, 0xff, file_len);
					m_fhead.macaddr[7] = 0;

					m_fhead.type=_nudata.image[idx].image_type;

					bResult = NUC_WritePipe(id, (unsigned char*)&m_fhead, sizeof(NORBOOT_MMC_HEAD));
					if (bResult == FALSE) {
						fprintf(stderr, "%s (%d) - NUC_WritePipe failed %d.\n", __func__, id, __LINE__);
						goto EXIT;
					}
					bResult = NUC_ReadPipe(id, (unsigned char *)&ack, (int)sizeof(unsigned int));
					if (bResult == FALSE) {
						fprintf(stderr, "%s (%d) - NUC_ReadPipe failed %d.\n", __func__, id, __LINE__);
						goto EXIT;
					}

					memcpy(lpBuffer, ddrbuf, ddrlen);
					fread(lpBuffer + ddrlen, 1, m_fhead.filelen, fp);
					break;
				}
				fclose(fp);
#if 1
				if (lpBuffer == NULL) {
					fprintf(stderr, "%s %d - lpBuffer is NULL!\n", __func__, __LINE__);
					goto EXIT;
				}
				
				fprintf(stdout, "Write image%d %s ...\n", idx, TypeT[_nudata.image[idx].image_idx].pName);
				pbuf = (unsigned char *)lpBuffer;
				scnt = file_len / BUF_SIZE;
				rcnt = file_len % BUF_SIZE;
				total = 0;
				while (scnt > 0) {
					bResult = NUC_WritePipe(id, (unsigned char*)pbuf, BUF_SIZE);
					if (bResult == FALSE) {
						fprintf(stderr, "%s (%d) - NUC_WritePipe failed %d.\n", __func__, id, __LINE__);
						goto EXIT;
					}
					pbuf += BUF_SIZE;
					total += BUF_SIZE;
					pos = (int)(((float)(((float)total / (float)file_len)) * 100));
					bResult = NUC_ReadPipe(id, (UCHAR *)&ack, 4);
					if ((bResult == FALSE) || (ack != BUF_SIZE)) {
						fprintf(stderr, "%s (%d) - NUC_ReadPipe failed %d.\n", __func__, id, __LINE__);
						goto EXIT;
					}
					scnt--;

					// fprintf(stdout, "Write image%d %s ... ",idx,TypeT[_nudata.image[idx].image_idx].pName);
					// show_progressbar(pos);
				}
				if (rcnt > 0) {
					bResult = NUC_WritePipe(id, (unsigned char*)pbuf, rcnt);
					if (bResult == FALSE) {
						fprintf(stderr, "%s (%d) - NUC_WritePipe failed %d.\n", __func__, id, __LINE__);
						goto EXIT;
					}
					total += rcnt;
					bResult = NUC_ReadPipe(id, (UCHAR *)&ack, 4);
					if (bResult == FALSE) {
						fprintf(stderr, "%s (%d) - NUC_ReadPipe failed %d.\n", __func__, id, __LINE__);
						goto EXIT;
					}
				}
				
				// fprintf(stdout, "Write image%d %s ... ", idx, TypeT[_nudata.image[idx].image_idx].pName);
				// show_progressbar(pos);
				bResult = NUC_ReadPipe(id, (UCHAR *)&blockNum, 4);
				if (bResult == FALSE) {
					fprintf(stderr, "%s (%d) - NUC_ReadPipe failed %d.\n", __func__, id, __LINE__);
					goto EXIT;
				}
				// show_progressbar(100);
				fprintf(stdout, "Write image%d %s ... Passed\n",idx,TypeT[_nudata.image[idx].image_idx].pName);
				
				if (_nudata.run == RUN_PROGRAM_VERIFY) {
					unsigned char	temp[BUF_SIZE];

					if (EnableOneWinUsbDevice(id) == FALSE) {
						fprintf(stderr, "%s %d (%d) - Failed to enable WinUsb device!\n", __func__, __LINE__, id);
						return ERR_CODE_USB_CONN;
					}

					NUC_SetType(0, MMC, (UCHAR *)&typeack, sizeof(typeack));
					
					m_fhead.flag = VERIFY_ACTION;
					if (_nudata.image[idx].image_type == IMG_T_LOADER)
						file_len = file_len - 32;
					
					bResult = NUC_WritePipe(id, (unsigned char*)&m_fhead, sizeof(NORBOOT_MMC_HEAD));
					if (bResult == FALSE) {
						fprintf(stderr, "%s (%d) - NUC_WritePipe failed %d.\n", __func__, id, __LINE__);
						goto EXIT;
					}
					bResult = NUC_ReadPipe(id, (unsigned char *)&ack, sizeof(unsigned int));
					if (bResult == FALSE) {
						fprintf(stderr, "%s (%d) - NUC_ReadPipe failed %d.\n", __func__, id, __LINE__);
						goto EXIT;
					}
					
					fprintf(stdout, "Verify image%d %s ...\n", idx, TypeT[_nudata.image[idx].image_idx].pName);
					pbuf = (unsigned char*)lpBuffer + m_fhead.initSize;
					scnt = (file_len - m_fhead.initSize) / BUF_SIZE;
					rcnt = (file_len - m_fhead.initSize) % BUF_SIZE;
					total = 0;
					while (scnt > 0) {
						bResult = NUC_ReadPipe(id, (unsigned char*)temp, BUF_SIZE);
						if (bResult == FALSE) {
							fprintf(stderr, "%s %d - Verify image%d %s... Failed,Result %d\n",
										__func__, __LINE__, idx, TypeT[_nudata.image[idx].image_idx].pName, bResult);
							fprintf(stderr, "2 scnt=%d\n", scnt);
							goto EXIT;
						}

						total += BUF_SIZE;
						if (DataCompare(temp, pbuf, BUF_SIZE))
							ack = BUF_SIZE;
						else
							ack = 0;  //compare error
						bResult = NUC_WritePipe(id, (unsigned char*)&ack, 4);
						if (bResult == FALSE) {
							fprintf(stderr, "Verify image%d %s... Failed\n",idx,TypeT[_nudata.image[idx].image_idx].pName);
							goto EXIT;
						}
						pos = (int)(((float)(((float)total / (float)file_len)) * 100));
						// show_progressbar(pos);
						pbuf += BUF_SIZE;

						scnt--;
					}

					if (rcnt > 0) {
						bResult = NUC_ReadPipe(id, (UCHAR *)temp, BUF_SIZE);
						if (bResult == FALSE) {
							fprintf(stderr, "%s %d - Verify image%d %s... Failed\n",
										__func__, __LINE__, idx,TypeT[_nudata.image[idx].image_idx].pName);
							goto EXIT;
						}
						
						total += rcnt;
						if (DataCompare(temp, pbuf, rcnt))
							ack = BUF_SIZE;
						else
							ack = 0;   //compare error
							
						bResult = NUC_WritePipe(id,(UCHAR *)&ack, 4);
						if ((bResult == FALSE) || (!ack)) {
							fprintf(stderr, "%s %d - Verify image%d %s... Failed\n",
										__func__, __LINE__, idx, TypeT[_nudata.image[idx].image_idx].pName);
							goto EXIT;
						}

						pos = (int)(((float)(((float)total / (float)file_len)) * 100));
						// fprintf(stdout, "Verify image%d %s ... ", idx, TypeT[_nudata.image[idx].image_idx].pName);
						// show_progressbar(pos);
					}
					// show_progressbar(100);
					fprintf(stdout, "Verify image%d %s... Passed\n",idx,TypeT[_nudata.image[idx].image_idx].pName);
				} //Verify_tag end
#endif
				free(lpBuffer);
			}
		}
	}
	return 0;

EXIT:
	fprintf(stderr, "\neMMC failed\n");
	return -1;
}

int UXmodem_Pack(int id)
{
	FILE	*fp;
	int		i, j;
	int		bResult, pos;
	unsigned int scnt, rcnt, file_len, ack, total = 0;
	unsigned char *pbuf;
	unsigned int  magic;
	unsigned char  *lpBuffer = NULL;
	char	temp[BUF_SIZE];
	char	buf[BUF_SIZE];
	PACK_HEAD *ppackhead;
	PACK_CHILD_HEAD child;
	int		posnum, burn_pos;
	unsigned int blockNum;
	NORBOOT_MMC_HEAD m_fhead;
	int     blockcnt, translen, reclen;
	USHORT	typeack;
	int		ddrlen;

	fprintf(stdout, "[%s]\n", __func__);
	
	if (fopen_s(&fp, _nudata.image[0].image_path, "rb") != 0) {
		fprintf(stderr, "%s %d - Open read File Error(-w %s) \n",
					__func__, __LINE__, _nudata.image[0].image_path);
		return -1;
	}

	fread((unsigned char *)&magic, 4, 1, fp);
	if (magic != 0x5) {
		fclose(fp);
		fprintf(stderr, "%s %d - Pack Image Format Error\n", __func__, __LINE__);
		goto EXIT;
	}

	fseek(fp, 0, SEEK_END);
	file_len = ftell(fp);
	fseek(fp, 0, SEEK_SET);
	
	lpBuffer = (unsigned char *)malloc(sizeof(unsigned char)*file_len); //read file to buffer
	memset(lpBuffer, 0xff, file_len);
	memset((unsigned char *)&m_fhead, 0, sizeof(NORBOOT_MMC_HEAD));

	/*--------------------------------------------------------------*/
	/*  Program                                                     */
	/*--------------------------------------------------------------*/
	if (_nudata.mode.id == MODE_SD) {
		((NORBOOT_MMC_HEAD *)&m_fhead)->flag = PACK_ACTION;
		((NORBOOT_MMC_HEAD *)&m_fhead)->type = _nudata.image[0].image_type;
		((NORBOOT_MMC_HEAD *)&m_fhead)->initSize = 0;
		((NORBOOT_MMC_HEAD *)&m_fhead)->filelen = file_len;
		bResult = NUC_WritePipe(id, (UCHAR *)&m_fhead, sizeof(NORBOOT_MMC_HEAD));
		if (bResult == FALSE) {
			fprintf(stderr, "%s (%d) - NUC_WritePipe failed %d.\n", __func__, id, __LINE__);
			goto EXIT;
		}
		bResult = NUC_ReadPipe(id, (unsigned char *)&ack, (int)sizeof(unsigned int));
		if (bResult == FALSE) {
			fprintf(stderr, "%s (%d) - NUC_ReadPipe failed %d.\n", __func__, id, __LINE__);
			goto EXIT;
		}
		fread(lpBuffer, 1, ((NORBOOT_MMC_HEAD *)&m_fhead)->filelen, fp);
	} else {
		m_fhead.flag = PACK_ACTION;
		((NORBOOT_NAND_HEAD *)&m_fhead)->type = _nudata.image[0].image_type;
		((NORBOOT_NAND_HEAD *)&m_fhead)->initSize = 0;
		((NORBOOT_NAND_HEAD *)&m_fhead)->filelen = file_len;
		bResult = NUC_WritePipe(id, (UCHAR *)&m_fhead, sizeof(NORBOOT_NAND_HEAD));
		if (bResult == FALSE) {
			fprintf(stderr, "%s (%d) - NUC_WritePipe failed %d.\n", __func__, id, __LINE__);
			goto EXIT;
		}
		bResult = NUC_ReadPipe(id,(unsigned char *)&ack,(int)sizeof(unsigned int));
		if (bResult == FALSE) {
			fprintf(stderr, "%s (%d) - NUC_ReadPipe failed %d.\n", __func__, id, __LINE__);
			goto EXIT;
		}
		fread(lpBuffer, 1, ((NORBOOT_NAND_HEAD *)&m_fhead)->filelen, fp);
	}

	pbuf = lpBuffer;
	ppackhead = (PACK_HEAD *)lpBuffer;
	bResult = NUC_WritePipe(id, (UCHAR *)pbuf, sizeof(PACK_HEAD));
	if (bResult == FALSE) {
		fprintf(stderr, "%s (%d) - NUC_WritePipe failed %d.\n", __func__, id, __LINE__);
		goto EXIT;
	}
	bResult = NUC_ReadPipe(id, (UCHAR *)&ack, 4);
	if (bResult == FALSE) {
		fprintf(stderr, "%s (%d) - NUC_ReadPipe failed %d.\n", __func__, id, __LINE__);
		goto EXIT;
	}
	
	total += sizeof(PACK_HEAD);
	pbuf += sizeof(PACK_HEAD);
	posnum = 0;
	for (i = 0; i < (int)(ppackhead->num); i++) {
		total = 0;
		memcpy(&child, (char *)pbuf, sizeof(PACK_CHILD_HEAD));
		bResult = NUC_WritePipe(id, (UCHAR *)pbuf, sizeof(PACK_CHILD_HEAD));
		if (bResult == FALSE) {
			fprintf(stderr, "%s (%d) - NUC_WritePipe failed %d.\n", __func__, id, __LINE__);
			goto EXIT;
		}

		if ((_nudata.mode.id == MODE_NAND) || (_nudata.mode.id == MODE_SD) || (_nudata.mode.id == MODE_SPINAND)) {
			bResult = NUC_ReadPipe(id, (UCHAR *)&ack, 4);
			if (bResult == FALSE) {
				fprintf(stderr, "%s (%d) - NUC_ReadPipe failed %d.\n", __func__, id, __LINE__);
				goto EXIT;
			}
		}
		pbuf += sizeof(PACK_CHILD_HEAD);
		
		fprintf(stdout, "Write pack image%d type %d, size = %d\n", i, child.imagetype, child.filelen);

		scnt = child.filelen / BUF_SIZE;
		rcnt = child.filelen % BUF_SIZE;
		total = 0;
		blockcnt = SPI_BLOCK_SIZE;

		while (scnt > 0) {
			// fprintf(stdout, "scnt = %d\n", scnt);
			bResult = NUC_WritePipe(id, (UCHAR *)pbuf, BUF_SIZE);
			if (bResult == FALSE) {
				fprintf(stderr, "%s (%d) - NUC_WritePipe failed %d.\n", __func__, id, __LINE__);
				goto EXIT;
			}
			pbuf += BUF_SIZE;
			total += BUF_SIZE;
			pos = (int)(((float)(((float)total / (float)child.filelen)) * 100));
			// show_progressbar(pos);
			bResult = NUC_ReadPipe(id, (UCHAR *)&ack, 4);
			if (bResult == FALSE) {
				fprintf(stderr, "%s (%d) - NUC_ReadPipe failed %d.\n", __func__, id, __LINE__);
				goto EXIT;
			}
			scnt--;
			if (_nudata.mode.id == MODE_SPINOR) {
				if ((blockcnt == total) && ( child.filelen > SPI_BLOCK_SIZE)) { //  (child.imagetype == IMG_T_LOADER)) {
					Sleep(1);
					bResult = NUC_ReadPipe(id, (UCHAR *)&ack, 4);
					if (bResult == FALSE) {
						fprintf(stderr, "%s (%d) - NUC_ReadPipe failed %d.\n", __func__, id, __LINE__);
						goto EXIT;
					}
					blockcnt += SPI_BLOCK_SIZE;
				}
			}
		}

		if (rcnt > 0) {
			bResult = NUC_WritePipe(id, (UCHAR *)pbuf, rcnt);
			if (bResult == FALSE) {
				fprintf(stderr, "%s (%d) - NUC_WritePipe failed %d.\n", __func__, id, __LINE__);
				goto EXIT;
			}
			pbuf += rcnt;
			total += rcnt;
			bResult = NUC_ReadPipe(id, (UCHAR *)&ack, 4);
			if (bResult == FALSE) {
				fprintf(stderr, "%s (%d) - NUC_ReadPipe failed %d.\n", __func__, id, __LINE__);
				goto EXIT;
			}
		}
		posnum += 100;
		if ((_nudata.mode.id == MODE_NAND) || (_nudata.mode.id == MODE_SD) || (_nudata.mode.id == MODE_SPINAND)) {
			bResult = NUC_ReadPipe(id, (UCHAR *)&blockNum, 4);
			if (bResult == FALSE) {
				fprintf(stderr, "%s %d (%d) - NUC_ReadPipe failed.\n", __func__, __LINE__, id);
				goto EXIT;
			}
		} else if (_nudata.mode.id == MODE_SPINOR) {
			burn_pos = 0;
			{  // while (burn_pos != 100) {
				bResult = NUC_ReadPipe(id, (UCHAR *)&ack, 4);
				if (bResult == FALSE) {
					fprintf(stderr, "%s %d (%d) - NUC_ReadPipe failed.\n", __func__, __LINE__, id);
					goto EXIT;
				}
				if (!((ack >> 16) & 0xffff)) {
					burn_pos = (UCHAR)(ack & 0xffff);
				} else {
					goto EXIT;
				}
				// fprintf(stdout, "burn_pos = %d\n", burn_pos);
			}
		}
	} // end of pack image loop

	/*--------------------------------------------------------------*/
	/*  Verify                                                     */
	/*--------------------------------------------------------------*/

	switch (_nudata.mode.id) {
		case MODE_NAND:
			NUC_SetType(0, NAND, (UCHAR *)&typeack, sizeof(typeack));
			break;
		case MODE_SPINAND:
			NUC_SetType(0, SPINAND, (UCHAR *)&typeack, sizeof(typeack));
			break;
		case MODE_SPINOR:
			NUC_SetType(0, SPI, (UCHAR *)&typeack, sizeof(typeack));
			break;
		case MODE_SD:
			NUC_SetType(0, MMC, (UCHAR *)&typeack, sizeof(typeack));
			break;
		default:
			fprintf(stderr, "%s %d - Unknow mode!\n", __func__, __LINE__);
			break;
	}

	ppackhead = (PACK_HEAD *)lpBuffer;
	ppackhead->fileLength = ((lpBuffer[7]&0xff) << 24 | (lpBuffer[6]&0xff) << 16 | (lpBuffer[5]&0xff) << 8 | (lpBuffer[4]&0xff));
	ppackhead->num = ((lpBuffer[11]&0xff) << 24 | (lpBuffer[10]&0xff) << 16 | (lpBuffer[9]&0xff) << 8 | (lpBuffer[8]&0xff));

	total = sizeof(PACK_HEAD);
	pbuf = lpBuffer + sizeof(PACK_HEAD);

	if (_nudata.mode.id == MODE_SD) {
		memset(&m_fhead, 0, sizeof(NORBOOT_MMC_HEAD));
		m_fhead.flag = PACK_VERIFY_ACTION;
		((NORBOOT_MMC_HEAD *)&m_fhead)->filelen = ((lpBuffer[19] & 0xff) << 24 | (lpBuffer[18] & 0xff) << 16 | (lpBuffer[17] & 0xff) << 8 | (lpBuffer[16] & 0xff));
    	((NORBOOT_MMC_HEAD *)&m_fhead)->flashoffset = ((lpBuffer[23] & 0xff) << 24 | (lpBuffer[22] & 0xff) << 16 | (lpBuffer[21] & 0xff) << 8 | (lpBuffer[20] & 0xff)); // child1 start address
    	((NORBOOT_MMC_HEAD *)&m_fhead)->type = (lpBuffer[27] << 24 | lpBuffer[26] << 16 | lpBuffer[25] << 8 | lpBuffer[24]); // child1 image type
		((NORBOOT_MMC_HEAD *)&m_fhead)->no = ppackhead->num;

		bResult = NUC_WritePipe(id, (UCHAR *)&m_fhead, sizeof(NORBOOT_MMC_HEAD));
		if (bResult == FALSE) {
			fprintf(stderr, "%s (%d) - NUC_WritePipe failed %d.\n", __func__, id, __LINE__);
			goto EXIT;
		}
		bResult = NUC_ReadPipe(id, (unsigned char *)&ack, (int)sizeof(unsigned int));
		if (bResult == FALSE) {
			fprintf(stderr, "%s (%d) - NUC_ReadPipe failed %d.\n", __func__, id, __LINE__);
			goto EXIT;
		}
	} else if ((_nudata.mode.id == MODE_SPINAND) || (_nudata.mode.id == MODE_SPINOR) ||
	           (_nudata.mode.id == MODE_NAND)) {
		memset(&m_fhead, 0, sizeof(NORBOOT_NAND_HEAD));
		m_fhead.flag = PACK_VERIFY_ACTION;
		((NORBOOT_NAND_HEAD *)&m_fhead)->filelen= ((lpBuffer[19]&0xff) << 24 | (lpBuffer[18]&0xff) << 16 | (lpBuffer[17]&0xff) << 8 | (lpBuffer[16]&0xff)); // child1 file len
		((NORBOOT_NAND_HEAD *)&m_fhead)->flashoffset = ((lpBuffer[23]&0xff) << 24 | (lpBuffer[22]&0xff) << 16 | (lpBuffer[21]&0xff) << 8 | (lpBuffer[20]&0xff)); // child1 start address
		((NORBOOT_NAND_HEAD *)&m_fhead)->type = ((lpBuffer[27]&0xff) << 24 | (lpBuffer[26]&0xff) << 16 | (lpBuffer[25]&0xff) << 8 | (lpBuffer[24]&0xff)); // child1 image type
		((NORBOOT_NAND_HEAD *)&m_fhead)->no = ppackhead->num;
		bResult = NUC_WritePipe(id, (UCHAR *)&m_fhead, sizeof(NORBOOT_NAND_HEAD));
		if (bResult == FALSE) {
			fprintf(stderr, "%s (%d) - NUC_WritePipe failed %d.\n", __func__, id, __LINE__);
			goto EXIT;
		}
		bResult = NUC_ReadPipe(id,(unsigned char *)&ack,(int)sizeof(unsigned int));
		if (bResult == FALSE) {
			fprintf(stderr, "%s (%d) - NUC_ReadPipe failed %d.\n", __func__, id, __LINE__);
			goto EXIT;
		}
	} else {
		m_fhead.flag = PACK_VERIFY_ACTION;
		((NORBOOT_NAND_HEAD *)&m_fhead)->type = _nudata.image[0].image_type;
		((NORBOOT_NAND_HEAD *)&m_fhead)->initSize = 0;
		((NORBOOT_NAND_HEAD *)&m_fhead)->filelen = file_len;
		bResult = NUC_WritePipe(id, (UCHAR *)&m_fhead, sizeof(NORBOOT_NAND_HEAD));
		if (bResult == FALSE) {
			fprintf(stderr, "%s %d (%d) - NUC_WritePipe failed.\n", __func__, __LINE__, id);
			goto EXIT;
		}
		bResult = NUC_ReadPipe(id,(unsigned char *)&ack,(int)sizeof(unsigned int));
		if (bResult == FALSE) {
			fprintf(stderr, "%s %d (%d) - NUC_ReadPipe failed.\n", __func__, __LINE__, id);
			goto EXIT;
		}
	}

	for (i = 0; i < (int)(ppackhead->num); i++) {
		memcpy(&child, (char *)pbuf, sizeof(PACK_CHILD_HEAD));
		bResult = NUC_WritePipe(id, (UCHAR *)pbuf, sizeof(PACK_CHILD_HEAD));
		if (bResult == FALSE) {
			fprintf(stderr, "%s %d (%d) - NUC_WritePipe failed.\n", __func__, __LINE__, id);
			goto EXIT;
		}

		bResult = NUC_ReadPipe(id, (UCHAR *)&ack, 4);
		if (bResult == FALSE) {
			fprintf(stderr, "%s %d (%d) - NUC_ReadPipe failed.\n", __func__, __LINE__, id);
			goto EXIT;
		}

		total += sizeof(PACK_CHILD_HEAD);
		pbuf += sizeof(PACK_CHILD_HEAD);

		if (child.imagetype == IMG_T_LOADER) {
			//ddrbuf=DDR2Buf(mainWnd->DDRBuf,mainWnd->DDRLen,&ddrlen);
			ddrlen = (((_nudata.ddr_sbuff_len + 8 + 15) / 16) * 16);
			pbuf += ddrlen + IBR_HEADER_LEN;
			total += ddrlen + IBR_HEADER_LEN;
			//TRACE(_T("len = %d    mainWnd->DDRLen = %d,  %d\n"), 16 + mainWnd->DDRLen + IBR_HEADER_LEN, mainWnd->DDRLen, 16 + ddrlen + IBR_HEADER_LEN);
			// send DDR parameter Length
			bResult = NUC_WritePipe(id, (UCHAR *)&ddrlen, 4);
			//Sleep(5);
			if (bResult == FALSE) {
				fprintf(stderr, "%s %d (%d) - NUC_WritePipe failed.\n", __func__, __LINE__, id);
				goto EXIT;
			}

			bResult = NUC_ReadPipe(id, (UCHAR *)&ack, 4);
			if(bResult==FALSE) {
				fprintf(stderr, "%s %d (%d) - NUC_ReadPipe failed.\n", __func__, __LINE__, id);
				goto EXIT;
			}
			blockcnt = (child.filelen - ddrlen - IBR_HEADER_LEN) / (SPI_BLOCK_SIZE);
		} else {
			blockcnt = child.filelen / (SPI_BLOCK_SIZE);
		}
		
		fprintf(stdout, "Verify pack image%d type %d, size = %d\n", i, child.imagetype, child.filelen);
		
		if (_nudata.mode.id == MODE_SPINOR) {
			for (j = 0; j < blockcnt; j++) {
				translen = SPI_BLOCK_SIZE;
				while (translen > 0) {
					fseek(fp, total, SEEK_SET);
					fread(temp, BUF_SIZE, 1, fp);

					bResult = NUC_ReadPipe(id, (UCHAR *)lpBuffer, BUF_SIZE);
					if (bResult == FALSE) {
						fprintf(stderr, "%s %d (%d) - NUC_ReadPipe failed.\n", __func__, __LINE__, id);
						goto EXIT;
					}

					total += BUF_SIZE;
					pbuf += BUF_SIZE;
        	
					if (DataCompare((unsigned char *)temp, (unsigned char *)lpBuffer, BUF_SIZE)) {
						ack = BUF_SIZE;
					} else {
						ack = 0;  //compare error
						fprintf(stderr, "Verify failed at file offset %d\n", total - BUF_SIZE);
						goto EXIT;
					}

					bResult = NUC_WritePipe(id, (UCHAR *)&ack, 4);
					if (bResult == FALSE) {
						fprintf(stderr, "%s %d (%d) - NUC_WritePipe failed.\n", __func__, __LINE__, id);
						goto EXIT;
					}
        	
					translen -= BUF_SIZE;
				}
			}   // for (j = 0; j < blockcnt; j++)
        	
			if (child.imagetype == IMG_T_LOADER) {
				translen = (child.filelen-ddrlen-IBR_HEADER_LEN) - (blockcnt*SPI_BLOCK_SIZE);
				// fprintf(stdout, "UBOOT remain: blockcnt=%d, child.filelen = 0x%x(%d)   ddrlen=%d  translen=%d\n", blockcnt, child.filelen, child.filelen, ddrlen, translen);
			} else {
				translen = child.filelen - (blockcnt * SPI_BLOCK_SIZE);
			}
        	
			// fprintf(stdout, "remain: blockcnt=%d, child.filelen = 0x%x(%d)     translen=%d\n", blockcnt, child.filelen, child.filelen, translen);
			if (translen > 0) {
				while (translen > 0) {
					reclen = MIN(BUF_SIZE, translen);
					fseek(fp, total, SEEK_SET);
					fread(temp, reclen, 1, fp);

					bResult = NUC_ReadPipe(id, (UCHAR *)lpBuffer, reclen);
					if (bResult == FALSE) {
						fprintf(stderr, "%s %d (%d) - NUC_ReadPipe failed.\n", __func__, __LINE__, id);
						goto EXIT;
					}
        	
					total += reclen;
					pbuf += reclen;
					translen -= reclen;
						
					// fprintf(stdout, "child.filelen = %d  total=%d, reclen=%d, translen=%d\n", child.filelen, total, reclen, translen);
						
					if (DataCompare((unsigned char *)temp, (unsigned char *)lpBuffer, reclen)) {
						ack = reclen;
					} else {
						ack = 0;  //compare error
						fprintf(stderr, "Verify failed at file offset %d\n", total - BUF_SIZE);
						goto EXIT;
					}
        	
					bResult = NUC_WritePipe(id, (UCHAR *)&ack, 4);
					if (bResult == FALSE) {
						fprintf(stderr, "%s %d (%d) - NUC_WritePipe failed.\n", __func__, __LINE__, id);
						goto EXIT;
					}
				}  // while (translen > 0)
			}  // if (translen > 0)
		
		} else {    // not SPINOR
			if(child.imagetype == IMG_T_LOADER) {
				scnt = (child.filelen - ddrlen - IBR_HEADER_LEN) / BUF_SIZE;
				rcnt = (child.filelen - ddrlen - IBR_HEADER_LEN) % BUF_SIZE;
			} else {
				scnt = child.filelen / BUF_SIZE;
				rcnt = child.filelen % BUF_SIZE;
			}
			
			while (scnt > 0) {
				//TRACE("scnt=0x%x(%d)\n", scnt, scnt);
				fseek(fp, total, SEEK_SET);
				fread(temp, BUF_SIZE, 1, fp);
			
				bResult = NUC_ReadPipe(id, (UCHAR *)buf, BUF_SIZE);
				if (bResult == FALSE) {
					fprintf(stderr, "%s %d (%d) - NUC_ReadPipe failed.\n", __func__, __LINE__, id);
					goto EXIT;
				}
			
				total += BUF_SIZE;
				pbuf += BUF_SIZE;
				
				if (DataCompare((unsigned char *)temp, (unsigned char *)buf, BUF_SIZE)) {
					ack = BUF_SIZE;
				} else {
					ack = 0;  //compare error
					fprintf(stderr, "Verify failed at file offset %d\n", total - BUF_SIZE);
					goto EXIT;
				}
				
				bResult = NUC_WritePipe(id, (UCHAR *)&ack, 4);
				if (bResult == FALSE) {
					fprintf(stderr, "%s %d (%d) - NUC_WritePipe failed.\n", __func__, __LINE__, id);
					goto EXIT;
				}
				scnt--;
			}
			
			int temp_len = 0;
			if (rcnt > 0) {
				memset((char *)&temp, 0xff, BUF_SIZE);
				if (child.imagetype != IMG_T_ENV) {
					fseek(fp, total, SEEK_SET);
					fread(temp, rcnt, 1, fp);
					temp_len = rcnt;
				} else {
					fseek(fp, total, SEEK_SET);
					fread(temp, BUF_SIZE, 1, fp);
					temp_len = BUF_SIZE;
				}
			
				//bResult=NucUsb.NUC_ReadPipe(id,(UCHAR *)lpBuffer,BUF_SIZE);
				bResult = NUC_ReadPipe(id, (UCHAR *)buf, BUF_SIZE);
				if (bResult == FALSE) {
					fprintf(stderr, "%s %d (%d) - NUC_ReadPipe failed.\n", __func__, __LINE__, id);
					goto EXIT;
				}
				
				total += temp_len;
				pbuf += temp_len;
				if (DataCompare((unsigned char *)temp, (unsigned char *)buf, temp_len)) {
					ack = BUF_SIZE;
				} else {
					ack = 0;  //compare error
					fprintf(stderr, "Verify failed at file offset %d\n", total - BUF_SIZE);
					goto EXIT;
				}
			
				bResult = NUC_WritePipe(id, (UCHAR *)&ack, 4);
				if (bResult == FALSE) {
					fprintf(stderr, "%s %d (%d) - NUC_WritePipe failed.\n", __func__, __LINE__, id);
					goto EXIT;
				}
			}
		}
	} // end of pack image loop

	fclose(fp);
	free(lpBuffer);
	// show_progressbar(100);
	fprintf(stdout, "Pack image ... Passed\n");
	return 0;
EXIT:
	fclose(fp);
	if (lpBuffer)
		free(lpBuffer);
	return -1;
}

static BOOL DDRtoDevice(int id, char *buf, unsigned int len)
{
	BOOL	bResult;
	char	*pbuf;
	unsigned int scnt, rcnt, ack;
	AUTOTYPEHEAD head;
	
	// fprintf(stdout, "[%s] (%d)\n", __func__, id);
	
	head.address = DDRADDRESS;  // 0x10
	head.filelen = len;

	bResult = NUC_WritePipe(id, (unsigned char*)&head, sizeof(AUTOTYPEHEAD));
	if (bResult == FALSE) {
		fprintf(stderr, "%s %d - NUC_WritePipe error. 0x%x\n", __func__, __LINE__, GetLastError());
		goto failed;
	}

	Sleep(5);
	pbuf = buf;

	scnt = len / BUF_SIZE;
	rcnt = len % BUF_SIZE;
	while (scnt > 0) {
		bResult = NUC_WritePipe(id, (UCHAR *)pbuf, BUF_SIZE);
		if (bResult != TRUE) {
			CloseWinUsbDevice(id);
			fprintf(stderr, "(%d) %s scnt%d error! 0x%x\n", id, __func__, scnt, GetLastError());
			return FALSE;
		}		

		bResult = NUC_ReadPipe(id, (UCHAR *)&ack, 4);
		if ((bResult == TRUE) && ((int)ack==(BUF_SIZE+1))) {
			// xub.bin is running on device
			CloseWinUsbDevice(id);
			return FW_CODE;
		}
		if ((bResult == FALSE) || ((int)ack == (BUF_SIZE+1))) {
				goto failed;
		}
		scnt--;
		pbuf += BUF_SIZE;
		fprintf(stdout, "scnt = %d\n", scnt);
	}

	if (rcnt > 0) {
		bResult = NUC_WritePipe(id, (UCHAR *)pbuf, rcnt);
		if (bResult != TRUE) {
			CloseWinUsbDevice(id);
			fprintf(stderr, "XXX (%d) %s rcnt%d error! 0x%x\n", id, __func__, rcnt, GetLastError());
			return FALSE;
		}

		bResult = NUC_ReadPipe(id,(UCHAR *)&ack,4);
		if ((bResult == TRUE) && ((int)ack==(BUF_SIZE+1))) {
			// xub.bin is running on device
			CloseWinUsbDevice(id);
			return FW_CODE;
		}
		if ((bResult == FALSE) || ((int)ack == (BUF_SIZE+1))) {
				goto failed;
		}
	}

	bResult = NUC_ReadPipe(id, (UCHAR *)&ack, 4);
	if ((bResult == TRUE) && (ack == 0x55AA55AA))
		return TRUE;

failed:
	CloseWinUsbDevice(id);
	return FALSE;
}

DWORD FWGetRamAddress(FILE* fp)
{
	BINHEADER head;
	UCHAR SIGNATURE[]= {'W','B',0x5A,0xA5};
	
	fread((CHAR*)&head, sizeof(head), 1, fp);
	
	if (head.signature == *(DWORD*)SIGNATURE) {
		return head.address;
	} else
		return 0xFFFFFFFF;
}

BOOL XUSB(int id, char *m_BinName)
{
	BOOL	bResult;
	CString posstr, str;
	CString tempstr;
	int		count = 0;
	FILE	*fp;
	int		fw_flag;
	int		pos = 0;
	AUTOTYPEHEAD fhead;
	XBINHEAD xbinhead;  // 16bytes for IBR using
	DWORD	version;
	unsigned int total, file_len, scnt, rcnt, ack;
	
	/***********************************************/
	fprintf(stdout, "[%s] %d, xusb file name: %s\n", __func__, id, m_BinName);
	
	bResult = EnableOneWinUsbDevice(id);
	if (bResult == FALSE) {
		fprintf(stderr, "%s %d (%d) USB Device Open error\n", __func__, __LINE__, id);
		return FALSE;
	}
	
	// fprintf(stdout, "(%d) %s, ddr_sbuff_len = %d \n", id, __func__, _nudata.ddr_sbuff_len);
	fw_flag = DDRtoDevice(id, _nudata.ddr_sbuff, _nudata.ddr_sbuff_len);
	if (fw_flag == FALSE) {
		_WinUsb.WinUsbNumber -= 1;
		CloseWinUsbDevice(id);
		fprintf(stderr, "%s - XXX (%d) DDRtoDevice failed\n", __func__, id);
		return FALSE;
	}
	
	ULONG cbSize = 0;
	unsigned char* lpBuffer = new unsigned char[BUF_SIZE];
	if (fopen_s(&fp, m_BinName, "rb") != 0) {
		delete []lpBuffer;
		CloseWinUsbDevice(id);
		fclose(fp);
		fprintf(stderr, "XXX Bin File Open error\n");
		return FALSE;
	}
	
	fread((char*)&xbinhead, sizeof(XBINHEAD), 1, fp);
	version = xbinhead.version;
	if (fw_flag == FW_CODE) {
		fclose(fp);
		delete []lpBuffer;
		CloseWinUsbDevice(id);
		return TRUE;
	}
	
	//Get File Length
	fseek(fp, 0, SEEK_END);
	file_len = ftell(fp); // - sizeof(XBINHEAD);
	fseek(fp, 0, SEEK_SET);
	
	if (!file_len) {
		fclose(fp);
		delete []lpBuffer;
		CloseWinUsbDevice(id);
		fprintf(stderr, "File length is zero\n");
		return FALSE;
	}
	
	fhead.filelen = file_len;
	fhead.address = FWGetRamAddress(fp);//0x8000;
	
	if (fhead.address == 0xFFFFFFFF) {
		delete []lpBuffer;
		CloseWinUsbDevice(id);
		fclose(fp);
		fprintf(stderr, "Invalid Image !\n");
		return FALSE;
	}
	
	memcpy(lpBuffer, (unsigned char*)&fhead, sizeof(fhead)); // 8 bytes
	bResult = NUC_WritePipe(id, (unsigned char*)&fhead, sizeof(fhead));
	if (bResult == FALSE) {
		delete []lpBuffer;
		CloseWinUsbDevice(id);
		fclose(fp);
		return FALSE;
	}
	scnt = file_len / BUF_SIZE;
	rcnt = file_len % BUF_SIZE;
	
	total = 0;
	// fprintf(stdout, "%s %d (%d) - file_len %d, FW scnt %d, rcnt = %d\n", __func__, __LINE__, id, file_len, scnt, rcnt);
	while (scnt > 0) {
		fread(lpBuffer, BUF_SIZE, 1, fp);
		bResult = NUC_WritePipe(id, lpBuffer, BUF_SIZE);
		if (bResult != TRUE) {
			delete []lpBuffer;
			CloseWinUsbDevice(id);
			fclose(fp);
			//fprintf(stderr, "XXX (%d) FW scnt error. 0x%x\n", id, GetLastError());
			return FALSE;
		}

		total += BUF_SIZE;
		scnt--;
		
		bResult = NUC_ReadPipe(id, (UCHAR *)&ack, 4);
		if ((bResult == FALSE) || ((int)ack != BUF_SIZE)) {
			delete []lpBuffer;
			CloseWinUsbDevice(id);
			fclose(fp);
			
			if ((bResult == TRUE) && ((int)ack == (BUF_SIZE+1))) {
				// xub.bin is running on device
				return TRUE;
			} else
				return FALSE;
		}
	}
	
	memset(lpBuffer, 0x0, BUF_SIZE);
	if (rcnt > 0) {
		fread(lpBuffer, rcnt, 1, fp);
		bResult = NUC_WritePipe(id, lpBuffer, BUF_SIZE);
		if(bResult != TRUE) {
			delete []lpBuffer;
			CloseWinUsbDevice(id);
			fclose(fp);
			fprintf(stderr, "XXX (%d) FW rcnt error. 0x%x\n", id, GetLastError());
			return FALSE;
		}

		total += rcnt;
		bResult = NUC_ReadPipe(id, (UCHAR *)&ack, 4);
		if (bResult == FALSE) {
			delete []lpBuffer;
			CloseWinUsbDevice(id);
			fclose(fp);
			return FALSE;
		}
		// fprintf(stdout, "Final ack is 0x%x\n", ack);
	}
	
	delete []lpBuffer;
	// CloseWinUsbDevice(id);
	fclose(fp);
	return TRUE;
}


#define CRC32POLY 0x04c11db7
static const unsigned long crc32_table[256] = {
	0x00000000, 0x77073096, 0xee0e612c, 0x990951ba, 0x076dc419,
	0x706af48f, 0xe963a535, 0x9e6495a3, 0x0edb8832, 0x79dcb8a4,
	0xe0d5e91e, 0x97d2d988, 0x09b64c2b, 0x7eb17cbd, 0xe7b82d07,
	0x90bf1d91, 0x1db71064, 0x6ab020f2, 0xf3b97148, 0x84be41de,
	0x1adad47d, 0x6ddde4eb, 0xf4d4b551, 0x83d385c7, 0x136c9856,
	0x646ba8c0, 0xfd62f97a, 0x8a65c9ec, 0x14015c4f, 0x63066cd9,
	0xfa0f3d63, 0x8d080df5, 0x3b6e20c8, 0x4c69105e, 0xd56041e4,
	0xa2677172, 0x3c03e4d1, 0x4b04d447, 0xd20d85fd, 0xa50ab56b,
	0x35b5a8fa, 0x42b2986c, 0xdbbbc9d6, 0xacbcf940, 0x32d86ce3,
	0x45df5c75, 0xdcd60dcf, 0xabd13d59, 0x26d930ac, 0x51de003a,
	0xc8d75180, 0xbfd06116, 0x21b4f4b5, 0x56b3c423, 0xcfba9599,
	0xb8bda50f, 0x2802b89e, 0x5f058808, 0xc60cd9b2, 0xb10be924,
	0x2f6f7c87, 0x58684c11, 0xc1611dab, 0xb6662d3d, 0x76dc4190,
	0x01db7106, 0x98d220bc, 0xefd5102a, 0x71b18589, 0x06b6b51f,
	0x9fbfe4a5, 0xe8b8d433, 0x7807c9a2, 0x0f00f934, 0x9609a88e,
	0xe10e9818, 0x7f6a0dbb, 0x086d3d2d, 0x91646c97, 0xe6635c01,
	0x6b6b51f4, 0x1c6c6162, 0x856530d8, 0xf262004e, 0x6c0695ed,
	0x1b01a57b, 0x8208f4c1, 0xf50fc457, 0x65b0d9c6, 0x12b7e950,
	0x8bbeb8ea, 0xfcb9887c, 0x62dd1ddf, 0x15da2d49, 0x8cd37cf3,
	0xfbd44c65, 0x4db26158, 0x3ab551ce, 0xa3bc0074, 0xd4bb30e2,
	0x4adfa541, 0x3dd895d7, 0xa4d1c46d, 0xd3d6f4fb, 0x4369e96a,
	0x346ed9fc, 0xad678846, 0xda60b8d0, 0x44042d73, 0x33031de5,
	0xaa0a4c5f, 0xdd0d7cc9, 0x5005713c, 0x270241aa, 0xbe0b1010,
	0xc90c2086, 0x5768b525, 0x206f85b3, 0xb966d409, 0xce61e49f,
	0x5edef90e, 0x29d9c998, 0xb0d09822, 0xc7d7a8b4, 0x59b33d17,
	0x2eb40d81, 0xb7bd5c3b, 0xc0ba6cad, 0xedb88320, 0x9abfb3b6,
	0x03b6e20c, 0x74b1d29a, 0xead54739, 0x9dd277af, 0x04db2615,
	0x73dc1683, 0xe3630b12, 0x94643b84, 0x0d6d6a3e, 0x7a6a5aa8,
	0xe40ecf0b, 0x9309ff9d, 0x0a00ae27, 0x7d079eb1, 0xf00f9344,
	0x8708a3d2, 0x1e01f268, 0x6906c2fe, 0xf762575d, 0x806567cb,
	0x196c3671, 0x6e6b06e7, 0xfed41b76, 0x89d32be0, 0x10da7a5a,
	0x67dd4acc, 0xf9b9df6f, 0x8ebeeff9, 0x17b7be43, 0x60b08ed5,
	0xd6d6a3e8, 0xa1d1937e, 0x38d8c2c4, 0x4fdff252, 0xd1bb67f1,
	0xa6bc5767, 0x3fb506dd, 0x48b2364b, 0xd80d2bda, 0xaf0a1b4c,
	0x36034af6, 0x41047a60, 0xdf60efc3, 0xa867df55, 0x316e8eef,
	0x4669be79, 0xcb61b38c, 0xbc66831a, 0x256fd2a0, 0x5268e236,
	0xcc0c7795, 0xbb0b4703, 0x220216b9, 0x5505262f, 0xc5ba3bbe,
	0xb2bd0b28, 0x2bb45a92, 0x5cb36a04, 0xc2d7ffa7, 0xb5d0cf31,
	0x2cd99e8b, 0x5bdeae1d, 0x9b64c2b0, 0xec63f226, 0x756aa39c,
	0x026d930a, 0x9c0906a9, 0xeb0e363f, 0x72076785, 0x05005713,
	0x95bf4a82, 0xe2b87a14, 0x7bb12bae, 0x0cb61b38, 0x92d28e9b,
	0xe5d5be0d, 0x7cdcefb7, 0x0bdbdf21, 0x86d3d2d4, 0xf1d4e242,
	0x68ddb3f8, 0x1fda836e, 0x81be16cd, 0xf6b9265b, 0x6fb077e1,
	0x18b74777, 0x88085ae6, 0xff0f6a70, 0x66063bca, 0x11010b5c,
	0x8f659eff, 0xf862ae69, 0x616bffd3, 0x166ccf45, 0xa00ae278,
	0xd70dd2ee, 0x4e048354, 0x3903b3c2, 0xa7672661, 0xd06016f7,
	0x4969474d, 0x3e6e77db, 0xaed16a4a, 0xd9d65adc, 0x40df0b66,
	0x37d83bf0, 0xa9bcae53, 0xdebb9ec5, 0x47b2cf7f, 0x30b5ffe9,
	0xbdbdf21c, 0xcabac28a, 0x53b39330, 0x24b4a3a6, 0xbad03605,
	0xcdd70693, 0x54de5729, 0x23d967bf, 0xb3667a2e, 0xc4614ab8,
	0x5d681b02, 0x2a6f2b94, 0xb40bbe37, 0xc30c8ea1, 0x5a05df1b,
	0x2d02ef8d
};

unsigned int CalculateCRC32(unsigned char * buf,unsigned int len)
{
	unsigned int i;
	unsigned int crc32;
	unsigned char* byteBuf;
	crc32 = 0 ^ 0xFFFFFFFF;
	byteBuf = (unsigned char *) buf;
	for (i=0; i < len; i++) {
		crc32 = (crc32 >> 8) ^ crc32_table[ (crc32 ^ byteBuf[i]) & 0xFF ];
	}
	return ( crc32 ^ 0xFFFFFFFF );
}

