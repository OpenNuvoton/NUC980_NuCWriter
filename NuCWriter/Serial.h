#ifndef __SERIAL_H
#define __SERIAL_H

#define IBR_HEADER_LEN          (32)
#define PACK_FOMRAT_HEADER_LEN  (48) // 12*4

#define READ_TIMEOUT			10000
#define MAX_FILESIZE			(50 * 1024 * 1024) //50M

#define BURN					0xb0
#define BURN_TYPE 				0x80

#define BUF_SIZE 				4096

enum {
	IMAGE_ACTIVE 		= 0, //(-a) 1<<0
	IMAGE_COPY2RAM		= 1, //(-c) 
	IMAGE_EXEC			= 2, //(-x) 
	IMAGE_FILE			= 3, //(-f) 
	IMAGE_COMPRESSED	= 4, //(-z) 
	IMAGE_ROMFS			= 5, //(-r)
	IMAGE_LINUX			= 6, //(-l)
	IMAGE_NOFOOTER		= 7, //
};

#define ADD				0
#define MODIFY			1
#define ERASE			2
#define FORMAT			3

enum IMG_TYPE {
	IMG_T_DATA 		= 0,
	IMG_T_ENV 		= 1,
	IMG_T_LOADER 	= 2,
	IMG_T_PACK 		= 3,
	IMG_T_PARTITION = 3,
	IMG_T_IMAGE 	= 4,
	IMG_T_DATA_OOB 	= 5,
};

#define	PMTP 			15

#define NAND_VERIFY_FILESYSTEM_ERROR -2
#define NAND_VERIFY_PACK_ERROR -3

#define BLOCK_16K		0x01
#define BLOCK_128K		0x02
#define BLOCK_256K		0x03

#define FILE_BLOCK		4096

#define SPI64K			(64 * 1024)
#define SPI63K			(63 * 1024)

#define NAND_SIZE		(64 * 2048)

#define MMC512B			512

typedef unsigned short USHORT;
typedef char CHAR;
typedef unsigned char		UCHAR;
typedef unsigned int		UINT32, *PUINT32;
typedef unsigned char		UINT8;
typedef unsigned short		UINT16;

#pragma pack(4)     /* set alignment to 1 byte boundary */
typedef struct _INFO_T {
	UINT32  Nand_uPagePerBlock;
	UINT32  Nand_uPageSize;
	UINT32  Nand_uSectorPerBlock;
	UINT32  Nand_uBlockPerFlash;
	UINT32  Nand_uBadBlockCount;
	UINT32  Nand_uSpareSize;
	UINT32  Nand_uIsUserConfig;

	UINT32  SPI_ID;
	UINT32  SPI_uIsUserConfig;
	UINT8   SPI_QuadReadCmd;
	UINT8   SPI_ReadStatusCmd;
	UINT8   SPI_WriteStatusCmd;
	UINT8   SPI_StatusValue;
	UINT8   SPI_dummybyte;

	UINT32  EMMC_uBlock;
	UINT32  EMMC_uReserved;

	UINT32  SPINand_uIsUserConfig;
	UINT32  SPINand_ID;
	UINT16  SPINand_PageSize;
	UINT16  SPINand_SpareArea;
	UINT8   SPINand_QuadReadCmd;
	UINT8   SPINand_ReadStatusCmd;
	UINT8   SPINand_WriteStatusCmd;
	UINT8   SPINand_StatusValue;
	UINT8   SPINand_dummybyte;
	UINT32  SPINand_BlockPerFlash;
	UINT32  SPINand_PagePerBlock;
	UINT8   SPINand_IsDieSelect;
} INFO_T,*PINFO_T;

typedef struct _PACK_CHILD_HEAD {
	DWORD   filelen;
	DWORD   startaddr;
	DWORD   imagetype;
	DWORD   reserve[1];
} PACK_CHILD_HEAD,*PPACK_CHILD_HEAD;

typedef struct _PACK_HEAD {
	DWORD   actionFlag;
	DWORD   fileLength;
	DWORD   num;
	DWORD   reserve[1];
} PACK_HEAD,*PPACK_HEAD;


typedef struct _MaxOffset {
	UINT32  maxoffset;
	UINT32  imageno;
} MaxOffset;

typedef struct _BINHEADER {
	DWORD 	signature; //'W' 'B' 0x5A, 0xA5
	DWORD 	address;
	DWORD 	length;
	DWORD 	resv;

} BINHEADER,*PBINHEADER;

typedef struct _NAND_BINHEADER {

	DWORD signature; //'W' 'B' 0x5A, 0xA5
	DWORD count;
	DWORD blocksize;

} NAND_BINHEADER, *PNAND_BINHEADER;

typedef enum _DOWNMETHOD {
	SDRAM = 0,
	NOR,
	RAWNOR,
	NAND,
	RAWNAND,
	MMC,
	RAWMMC,
	SPI,
	RAWSPI,
	SPINAND,
	INFO,
	NOR_IMAGELIST,
	NAND_IMAGELIST,
	MMC_IMAGELIST,
	SPI_IMAGELIST,
	NORTYPE,
} DOWNMETHOD;

#define WRITE_ACTION		0
#define MODIFY_ACTION		1
#define ERASE_ACTION		2
#define VERIFY_ACTION		3
#define READ_ACTION			4
#define PACK_ACTION  		5
#define FORMAT_ACTION		6
#define PACK_VERIFY_ACTION	7

#define NEED_AUTORUN		0x80000000

typedef struct _SDRAM_RAW_TYPEHEAD { //SDRAM ,NOR(raw) and SPI
	DWORD flag;//write,modify and erase
	DWORD filelen;
	DWORD address; //if bit 31 is "1" ,autorun it
	DWORD dtbaddress;

} SDRAM_RAW_TYPEHEAD,*PSDRAM_RAW_TYPEHEAD;


typedef struct _MMC_RAW_TYPEHEAD { //SDRAM ,NOR(raw) and SPI
	DWORD flag;//write,modify and erase
	DWORD filelen;
	DWORD address; //if bit 31 is "1" ,autorun it
	DWORD formatsize;

} MMC_RAW_TYPEHEAD,*PMMC_RAW_TYPEHEAD;

typedef struct _AUTOTYPEHEAD {
	DWORD filelen;
	DWORD address;

} AUTOTYPEHEAD,*PAUTOTYPEHEAD;

typedef struct _XBINHEAD {
	DWORD sign;
	DWORD address;
	DWORD length;
	DWORD version;

} XBINHEAD,*PXBINHEAD;


typedef struct _NORBOOT_NAND_HEAD { //NAND and NOR(bootloader)
	DWORD flag;//write,modify and erase
	DWORD filelen;
	DWORD no;
	CHAR name[16];
	DWORD type;
	DWORD execaddr;    //end block
	DWORD flashoffset; //start block
	DWORD endaddr;
	UCHAR macaddr[8];  //mac address
	DWORD initSize;
} NORBOOT_NAND_HEAD,*PNORBOOT_NAND_HEAD;

typedef struct _NORBOOT_MMC_HEAD { //MMC(bootloader)
	DWORD flag;//write,modify and erase
	DWORD filelen;
	DWORD no;
	CHAR name[16];
	DWORD type;
	DWORD execaddr;
	DWORD flashoffset;
	DWORD endaddr;
	DWORD ReserveSize;  //unit of sector
	UCHAR macaddr[8];
	DWORD initSize;
	UCHAR  FSType;
	DWORD  PartitionNum;
	DWORD  Partition1Size;  //unit of MB
	DWORD  Partition2Size;  //unit of MB
	DWORD  Partition3Size;  //unit of MB
	DWORD  Partition4Size;  //unit of MB
	DWORD  PartitionS1Size; //Sector size unit 512Byte
	DWORD  PartitionS2Size; //Sector size unit 512Byte
	DWORD  PartitionS3Size; //Sector size unit 512Byte
	DWORD  PartitionS4Size; //Sector size unit 512Byte
} NORBOOT_MMC_HEAD,*PNORBOOT_MMC_HEAD;

typedef struct _NORBOOT_MTP_HEAD { //MMC(bootloader)
	UCHAR 	Mode;
	UCHAR	Option;
	UCHAR   Lock;
	UCHAR   Reserved[2];
	DWORD	KeyLen;
	DWORD	KeyOTP[8];
} NORBOOT_MTP_HEAD,*PNORBOOT_MTP_HEAD;

typedef struct _NANDROM_HEAD { //NAND ROM Head
	DWORD no;
	CHAR name[16];
	DWORD type;
	DWORD execaddr;
	DWORD startblock;
	BINHEADER head;

} NANDROM_HEAD,*PNANDROM_HEAD;


typedef struct _SPIIMAGE_HEAD {

	USHORT no;
	USHORT type;
	DWORD startoffset;
	DWORD execaddr;
	DWORD filelen;
	CHAR name[16];
} SPIIMAGE_HEAD,*PSPIIMAGE_HEAD;


typedef struct _MMCIMAGE_HEAD {

	USHORT no;
	USHORT type;
	DWORD startoffset;
	DWORD execaddr;
	DWORD filelen;
	CHAR name[16];
} MMCIMAGE_HEAD,*PMMCIMAGE_HEAD;

typedef struct _MMCROM_HEAD {

	DWORD signature0; //'W' 'B' 0x55, 0xAA
	DWORD count; // total image count (include image 0)
	DWORD resv;
	DWORD signature1;//'W' 'B' 0x59, 0x63

} MMCROM_HEAD,*PMMCROM_HEAD;


#endif	// __SERIAL_H
