#ifndef _HAVE_NUCWRITER_H
#define _HAVE_NUCWRITER_H

#include "Serial.h"

#define NUCWRITER_VER		"1.0.1"
#define NUDATA_PATH			"../sys_cfg"

#define USB_VENDOR_ID		0x0416	/* USB vendor ID used by the device */
#define USB_PRODUCT_ID		0x5963	/* USB product ID used by the device */

#define USB_ENDPOINT_IN		(LIBUSB_ENDPOINT_IN  | 1)	/* endpoint address */
#define USB_ENDPOINT_OUT	(LIBUSB_ENDPOINT_OUT | 2)	/* endpoint address */
#define USB_TIMEOUT			(10000)	/* Connection timeout (in ms) */

#define SPINAND_ENV_LEN		0x20000

#define USB_CONN_RETRY_MAX	5

enum err_code {
	ERR_CODE_USB_NODEV		= -1,
	ERR_CODE_USB_CONN		= -2,
	ERR_CODE_ACK_ERR		= -3,
	ERR_CODE_TIMEOUT		= -4,
	ERR_CODE_PROGRAM		= -20,
	ERR_CODE_VERIFY			= -21,
	ERR_CODE_ERASE			= -22,
	ERR_CODE_INVAL_ADDR		= -23,
	ERR_CODE_INI_FILE		= -30,
	ERR_CODE_IN_FILE		= -31,
	ERR_CODE_OUT_FILE		= -32,
	ERR_CODE_OPEN_FILE_IMG	= -33,
};

enum mpu_type {
	MPU_NUC970,
	MPU_NUC980,
	MPU_MA35D1,
};

enum {
	USBD_FLASH_SDRAM		= 0x0,
	USBD_FLASH_NAND			= 0x3,
	USBD_FLASH_NAND_RAW		= 0x4,
	USBD_FLASH_MMC			= 0x5,
	USBD_FLASH_MMC_RAW		= 0x6,
	USBD_FLASH_SPI			= 0x7,
	USBD_FLASH_SPI_RAW		= 0x8,
	USBD_MTP				= 0x9,
	USBD_INFO				= 0xA,
	USBD_BURN_TYPE			= 0x80,
};

#define DDRADDRESS			16
#define BUF_SIZE			4096

#define FW_CODE				2

#define SPI_BLOCK_SIZE		(64*1024)

#define MAX_IMAGE			12

enum {
	MODE_SDRAM				= 0,
	MODE_NAND				= 1,
	MODE_SPINOR				= 2,
	MODE_SPINAND			= 3,
	MODE_SD					= 4
};

#define DOWNLOAD			0x10
#define DOWNLOAD_RUN		0x11

enum {
	RUN_PROGRAM				= 0x21,
	RUN_PROGRAM_VERIFY		= 0x24,
	RUN_READ				= 0x22,
	RUN_ERASE				= 0x23,
	RUN_FORMAT				= 0x25
};

#define MAX_DEV				8

typedef struct _MODE_T {
	int		id;
	char		pName[64];
} MODE_T, *PMODE_T;

typedef struct _SDRAM_T {
	char			sdram_path[MAX_PATH];
	char			dtb_path[MAX_PATH];
	unsigned int	opetion;
	unsigned int	dtb_addr;
	unsigned int	exe_addr;
} SDRAM_T, *PSDRAM_T;

typedef struct _IMAGE_T {
	int				image_idx;
	int				image_type;
	char			image_path[MAX_PATH];
	unsigned int	image_exe_addr;
	unsigned int	image_start_offset;
} IMAGE_T,*PIMAGE_T;

typedef struct _ERASE_T {
	unsigned int	isall;  //0: yes, 1: no
	unsigned int	start_blocks;
	unsigned int	offset_blocks;
} ERASE_T, *PERASE_T;

typedef struct _READ_T {
	char			path[MAX_PATH];
	unsigned int	start_blocks;
	unsigned int	offset_blocks;
} READ_T, *PREAD_T;

typedef struct _NU_DATA_T {
	enum mpu_type   mpu;
	MODE_T			mode;
	unsigned int	run;
	SDRAM_T			sdram;
	int				image_num;
	char			ddr_file_name[64];
	char			ddr_path[MAX_PATH];
	IMAGE_T			image[MAX_IMAGE];
	INFO_T			user_def;
	ERASE_T			erase;
	READ_T			read;
	char			ddr_sbuff[BUF_SIZE];	// ddr settings read from DDR.ini
	int				ddr_sbuff_len;
} NU_DATA_T;

typedef struct _DEV_CTRL_T {
	//unsigned int	csg_usb_index;
	// libusb_device *dev_arr[MAX_DEV];
	int			dev_count;	
	bool		enable_all_device;
	bool		timeoutflag[MAX_DEV];
} DEV_CTRL_T;

typedef struct _PACK_T {
	unsigned int	enable_pack;
	char			pack_path[MAX_PATH];
} PACK_T, *PPACK_T;

extern struct _NU_DATA_T  _nudata;
extern struct _DEV_CTRL_T  _devctrl;
extern struct _PACK_T _pack;
extern NORBOOT_MMC_HEAD _mmc_head;

extern MODE_T ModeT[];
extern MODE_T TypeT[];
extern MODE_T RunT[];

#define RUN_ON_XUSB		0x08FF0001

#define sizearray(a)    (sizeof(a) / sizeof((a)[0]))

extern void usleep(unsigned int usec);
extern int  ParseIniFile(char *inifile);
extern unsigned int CalculateCRC32(unsigned char * buf,unsigned int len);
extern BOOL XUSB(int id, char *m_BinName);
extern int UXmodem_SDRAM(int id);
extern int UXmodem_NAND(int id);
extern int UXmodem_SPINOR(int id);
extern int UXmodem_SD(int id);
extern int UXmodem_SPINAND(int id);
extern int UXmodem_Pack(int id);
extern int UXmodem_PackImage(int id);


#endif
