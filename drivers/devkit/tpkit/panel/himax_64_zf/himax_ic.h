/* Himax Android Driver Sample Code for Himax chipset
 *
 * Copyright (C) 2021 Himax Corporation.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef HIMAX_IC_H
#define HIMAX_IC_H

#include <asm/segment.h>
#include <linux/atomic.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/async.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <linux/input/mt.h>
#include <linux/firmware.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/buffer_head.h>
#include <linux/pm_wakeup.h>
#include <linux/seq_file.h>
#include <linux/proc_fs.h>
#include "himax_platform.h"
#include <linux/regulator/consumer.h>
#include <linux/debugfs.h>
#include <linux/stat.h>
#include "huawei_ts_kit.h"

#define HX_DL_COORD_FORMAT "P%3d = 0x%2.2X,P%3d = 0x%2.2X,P%3d = 0x%2.2X,P%3d = 0x%2.2X\n"
#define PRT_LOG "Finger %d=> X:%d, Y:%d W:%d\n"

#if defined(CONFIG_FB)
#include <linux/notifier.h>
#include <linux/fb.h>
#endif

#ifdef CONFIG_OF
#include <linux/of_gpio.h>
#endif

#define MAX_DIAG_BUF_SIZE (4096 * 4)
#define HX_RING_BUF
#define UPDATE_FAIL		0
#define UPDATE_PASS		1
#define UPDATE_DONE		0
#define UPDATE_ONGOING		1

#define NOT_SUPPORT	0
#define SUPPORT	1
#define ID_NAME_LEN 3
#define ID_ADDR_LEN 4
#define FLASH_ADDR_LEN 14
#define SMWP_EN 1
#define SMWP_ON 1
#define SMWP_OFF 0
#define GLOVE_EN 1
#define FUNC_EN 1
#define FUNC_DIS 0
#define FW_RETRY_TIMES 3
#define ALL_ZERO_2_TIMES 2
#define ALL_ZERO_3_TIMES 3
#define ALL_ZERO_4_TIMES 4
#define HX_DDREG_FORMAT 0x30000000
#define HX_DDREG_REG_IDX 12
#define HX_DDREG_BANK_IDX 8
#define RAWDATA_NUM_OF_TRX_MAX 100
//use for write/read register
#define CRC_LEN							0x0099
#define RESERVED_VALUE					0x00
#define ADDR_AHB						0x0D
#define DATA_AHB						0x10
#define DATA_AHB_AUTO					0x11
#define DUMMY_REGISTER					0x08
#define ADDR_EN_BURST_MODE				0x13
#define DATA_EN_BURST_MODE				0x31
#define ADDR_BURST_READ					0x11
#define DATA_BURST_READ_OFF				0x00
#define DATA_BURST_READ_ON				0x01
#define ADDR_SENSE_SWITCH_1				0x31
#define DATA_SENSE_SWITCH_1_OFF			0x27
#define ADDR_SENSE_SWITCH_2				0x32
#define DATA_SENSE_SWITCH_2_OFF			0x95
#define DATA_SENSE_SWITCH_ON			0x00
#define ADDR_FLASH_BURNED				0x00
#define ADDR_READ_ACCESS				0x0C
#define DATA_READ_ACCESS				0x00
#define ADDR_READ_EVENT_STACK			0x30
#define DATA_DIAG_TEST_RESULT			0x00
#define DATA_0A_STATUS_OK				0x9D
#define fw_addr_ulpm_33                     0x33
#define fw_addr_ulpm_34                     0x34
#define fw_data_ulpm_11                     0x11
#define fw_data_ulpm_22                     0x22
#define fw_data_ulpm_33                     0x33
#define fw_data_ulpm_aa                     0xAA
//#define ADDR_IC_TYPE				0x900000D0
#define ADDR_READ_FW_VER			0x10007004
#define ADDR_READ_CONFIG_VER		0x10007084
#define ADDR_READ_CID_VER			0x10007000
#define ADDR_GLOVE_EN				0x10007F14
#define DATA_GLOVE_EN				0xA55AA55A
#define ADDR_SMWP_EN				0x10007F10
#define DATA_SMWP_EN				0xA55AA55A
#define ADDR_RESET_TCON				0x80020020
#define DATA_RESET_TCON				0x00000001
#define ADDR_SENSE_ON				0x90000098
#define DATA_SENSE_ON				0x00000053
#define ADDR_AHBSPI_SYSRST			0x90000018
#define DATA_AHBSPI_SYSRST			0x00000055
#define ADDR_SCU_POWER_STATE		0x900000A0
#define ADDR_SPI_FORMAT				0x80000010
#define DATA_SPI_FORMAT				0x00020780
#define ADDR_SPI_CONTROL			0x80000020
#define DATA_SPI_CONTROL			0x42000003
#define DATA_SET_PAGE_256			0x610FF000
#define DATA_SET_PAGE_256_READ		0x694002FF
#define DATA_WRITE_EN				0x47000000
#define DATA_BLK_WRITE_EN			0x67000000
#define ADDR_SPI_WREN				0x80000024
#define ADDR_SPI_CMD				0x80000028
#define DATA_SPI_CMD				0x00000005
#define DATA_ERASE_PRE				0x00000006
#define DATA_BLK_ERASE				0x000000D8
#define DATA_ERASE					0x000000C7
#define ADDR_SPI_RESD_STATUS		0x8000002C
#define DATA_SPI_RESD_STATUS		0xFFFFFFFF
#define DATA_RETURN_SRAM_EVENT		0x11223344
#define DATA_NOR_MODE				0x00
#define DATA_SORT_MODE				0xAA
#define ADDR_READ_MODE_CHK			0x900000A8
#define ADDR_IDLE_MODE				0x10007088
#define ADDR_DIAG_REG_SET			0x100072EC
#define ADDR_MKEY					0x100070E8
#define ADDR_DSRAM_START			0x10000000
#define DATA_DSRAM_START			0x00005AA5
#define ADDR_READ_CFG_FW_STATUS		0x10007F44
#define ADDR_READ_REG_FW_STATUS		0x900000E8
#define ADDR_READ_FW_BUG_MSG_ADDR	0x10007f40
#define ADDR_MAC_DC					0x10007FC8
#define ADDR_SET_READ_START			0x80000028
#define DATA_BURNING				0x8000002C
#define DATA_WRITE_CMD				0x00000002
#define DATA_WRITE_CMD_x3B			0x0000003B
#define ADDR_CRC_DATAMAXLEN_SET		0x80050028
#define ADDR_CRC_STATUS_SET			0x80050000
#define ADDR_CRC_RESULT				0x80050018
#define ADDR_NFRAME_SEL				0x10007294
#define DATA_NFRAME_SEL				0x0000000A
#define DATA_SORT_MODE_NFRAME		0x00000002

//=========HIMAX CHIPSET================
#define addr_normal_noise_thx   	0x1000708C
#define addr_noise_scale        	0x10007094
#define addr_recal_thx          	0x10007090
#define addr_palm_num           	0x100070A8
#define addr_weight_sup         	0x100072C8
#define addr_normal_weight_a    	0x1000709C
#define addr_weight_b           	0x10007094
#define ADDR_SORTING_MODE_SWITCH	0x10007F04
#define ADDR_STP_HNDSHKG			0x10000000
#define ADDR_HW_STST_CHK			0x900000E4
#define ADDR_RESET_ADC				0x80020094
#define DATA_RESET_ADC				0x00000001

#define DATA_HAND_SHAKING 0x00006AA6
#define ADDR_SET_CRITERIA 0x10007F1C
#define DATA_SET_IIR_FRM 0x00000190
#define ADDR_SWITCH_FLASH_RLD 0x100072C0
#define ADDR_SWITCH_FLASH_RLD_STS 0x10007F00
#define DATA_DISABLE_FLASH_RLD 0x0000A55A
#define DATA_DISABLE_ZF_FLASH_RLD 0x00009AA9
#define ADDR_OS_TEST_RESULT 0x10007F24
#define DATA_INIT 0x00000000
#define TMP_DATA 0x00000001
#define ADDR_TXRX_INFO 0x100070F4
#define ADDR_XY_RVRS 0x100070FA
#define ADDR_TP_RES 0x100070FC

#define FW_ADDR_FW_DBG_MSG_ADDR 0x10007f40
#define FW_ADDR_CHK_FW_STATUS 0x900000a8
#define FW_ADDR_CHK_DD_STATUS 0x900000E8
#define FW_ADDR_FLAG_RESET_EVENT 0x900000e4

#define FW_ADDR_ROI_SWITCH	0x1000745C
#define FW_ADDR_ROI_SWITCH_ON	0xA55AA55A
#define FW_ADDR_ROI_SWITCH_OFF	0x00000000

#ifdef HX_EN_MUT_BUTTON
#define ADDR_BT_NUM 0x080000E8
#endif
#define HX_ROI_ENABLE	1
#define HX_ROI_DISABLE	0
#define HX_ROI_EN_PSD	0x05

#define SIZE_1ST_PKG	66
#define SIZE_2ND_PKG	36
#define SIZE_HX_HEADER	4
#define IDX_PKG_NUM	2

#define ENTER_SAVE_MODE 0x0C
#define NOR_READ_LENGTH 128
#define MAX_READ_LENTH 1024
#define NODE_DD_DEBUG_MAX_LEN 20
#define FW_VENDOR_MAX_STR_LEN 128
#define FW_PAGE_SZ			128
#define HX1K					0x400
#define HX4K					0x1000
#define HX_32K_SZ				0x8000
#define HX_40K_SZ				0xA000
#define HX_48K_SZ				0xC000
#define DSRAM_SIZE HX_32K_SZ

//use for parse dts
#define HIMAX_DRIVER_VER "#5625_2023.11.27.01"
#define HIMAX_PRODUCT_NAME "Himax"
#define STR_IC_VENDOR "HIMAX"
#define HIMAX_TPCOLOR_LEN 2
#define HIMAX_ACTUAL_PROJECTID_LEN 9
#define TP_COLOR_SIZE 15
#define TS_WAKE_LOCK_TIMEOUT (5 * HZ)
#define FLASH_DUMP_FILE "/data/user/Flash_Dump.bin"
#define HX_FW_NAME "ts/touch_screen_firmware.bin"
#define CSVFILE_USE_SYSTEM_TYPE "himax,csvfile_use_system"
#define TEST_CAPACITANCE_VIA_CSVFILE "himax,test_capacitance_via_csvfile"
//use for function
#define NO_ERR 0
#define HX_ERR -1
#define SPI_FAIL (-1)
#define FW_NOT_READY 1
#define MODE_ND_CHNG 1
#define NORMAL_MODE 1
#define SORTING_MODE 2
#define MODE_CHANGE_NOREADY 4
#define HX_RECEIVE_BUF_MAX_SIZE 128
#define HX_RECEIVE_ROI_BUF_MAX_SIZE 164
#define HX_TOUCH_SIZE 56
#define HX_ROI_DATA_LENGTH 49
#define CSV_PRODUCT_SYSTEM 1
#define CSV_ODM_SYSTEM 2
#define ALLC_MEM_ERR 0xFF
#define HX_MAX_WRITE_SZ    (8 * 1024)

#define HX_COORDS_MAX_SIZE 4
#define HX_PROJECT_ID_LEN 10
#define CHIP_NAME_LEN 8
#define ABS_X_MAX_DEFAULT 720
#define ABS_Y_MAX_DEFAULT 1600
#define HX_COORD_BYTE_NUM 4
#define IRQ_DISABLE 0
#define IRQ_ENABLE 1
#define HW_RST_FLAT_ENABLE 1
#define TIME_OUT_200MS (5 * HZ)
#define WORK_QUE_TIME_100MS ((1 / 10) * HZ)
#define WORK_QUE_TIME_20MS ((2 * HZ) / 100)
/*Gesture register(0xd0) value*/
#define DOUBLE_CLICK_WAKEUP			(0x80)
#define SPECIFIC_LETTER_W			(0x0b)
#define SPECIFIC_LETTER_M			(0x07)
#define SPECIFIC_LETTER_E			(0x0c)
#define SPECIFIC_LETTER_C			(0x05)
#define LETTER_LOCUS_NUM	6
#define LINEAR_LOCUS_NUM	2
#define GEST_PTLG_ID_LEN	4
#define GEST_PTLG_HDR_LEN	4
#define GEST_PTLG_HDR_ID1				0xCC
#define GEST_PTLG_HDR_ID2				0x44
#define GEST_PT_MAX_NUM					128
#define IS_APP_ENABLE_GESTURE(x)		((u32)(1<<x))
#define LEFT_MOV_8BIT		8
#define LEFT_MOV_16BIT		16
#define LEFT_MOV_24BIT		24
#define RIGHT_MOV_8BIT		8
#define RIGHT_MOV_16BIT		16
#define RIGHT_MOV_24BIT		24
#define HEX_ONE_HUNDRED		0x100
#define HEX_ONE_MILLION		0x1000000
#define SHIFT_NUM			256
#define HEX_NUM0			0x00
#define HEX_NUM1			0x01
#define HEX_NUM2			0x02
#define HEX_NUM3			0x03
#define HEX_NUM4			0x04
#define TMP_DATA0			0x00
#define TMP_DATA1			0x99
#define TMP_ADDR_DEF0		0xE4
#define TP_INF_DATA_0		0xA3
#define TP_INF_DATA_1		0x3A
#define TP_INF_DATA_4		0xC0
#define TP_INF_DATA_5		0x72
#define CNT					100
#define SENSE_ON_0			0x00
#define SENSE_ON_1			0x01
#define SET_ON				1
#define SET_OFF				0
#define DATA_SIZE			64
#define RETRY_TIMES			200
#define CRC_ADDR					0x80050020
#define ADDR_AC_SWITCH_HIMAX		0x10007F38
#define DATA_SET_AC_ON				0xA55AA55A
#define DATA_SET_AC_OFF				0x00000000
#define ADDR_ESD_STATUS_HX83102E	0x10007fe4
#define ADDR_HORIZON_SWITCH_HIMAX	0x10007F3C
#define VERTICAL_SCREEN_STATUS		0xA55AA55A
#define HORIZONTAL_SCREEN_STATUS	0xA33AA33A


enum data_num {
	DATA_0 = 0,
	DATA_1,
	DATA_2,
	DATA_3,
	DATA_4,
	DATA_5,
	DATA_6,
	DATA_7,
	DATA_8,
};
enum himax_zf_update_type {
	HX_RESUME_UPDATE = 0,
	HX_BOOT_UPDATE,
	HX_FILE_UPDATE,
};

enum himax_hand_shaking_result {
	HX_HAND_SHAKING_RUNNING = 0,
	HX_HAND_SHAKING_STOP,
	HX_HAND_SHAKING_SPI_FAIL,
};
enum start_get_rawdata_from_event_result {
	HX_OK = 0,
	HX_ERROR,
};
enum himax_HW_reset_eunm {
	HX_LOADCONFIG_EN = true,
	HX_LOADCONFIG_DISABLE = false,
	HX_INT_EN = true,
	HX_INT_DISABLE = false,
};

enum bin_desc_map_table {
	TP_CONFIG_TABLE = 0x00000A00,
	FW_CID = 0x10000000,
	FW_VER = 0x10000100,
	CFG_VER = 0x10000600,
};


/* origin is 20/50 */
#define RST_LOW_PERIOD_S 5000
#define RST_LOW_PERIOD_E 5100
#define RST_HIGH_PERIOD_S 5000
#define RST_HIGH_PERIOD_E 5100

#define HX_SLEEP_6S         6000
#define HX_SLEEP_2S         2000
#define HX_SLEEP_1S         1000
#define HX_SLEEP_200MS		200
#define HX_SLEEP_120MS		120
#define HX_SLEEP_100MS		100
#define HX_SLEEP_50MS		50
#define HX_SLEEP_40MS		40
#define HX_SLEEP_30MS		30
#define HX_SLEEP_20MS		20
#define HX_SLEEP_10MS		10
#define HX_SLEEP_5MS		5
#define HX_SLEEP_2MS		2
#define HX_SLEEP_1MS		1
#define RAWDATA_GET_TIME_DEFAULT 0

//R44:
//1st parameter:reserve reserve reserve BYTES_ADDR[4:0]
//2st parameter:reserve reserve SECTOR_ADDR[10:5]
//3st parameter:reserve reserve reserve reserve reserve PAGE_ADDR[13:11]

#define	HX83108A_RX_NUM				32
#define	HX83108A_TX_NUM				18
#define	HX83108A_BT_NUM				0
#define	HX83108A_X_RES				720
#define	HX83108A_Y_RES				1600
#define	HX83108A_MAX_PT				10
#define	HX83108A_XY_REVERSE			false
#define	HX83108A_INT_IS_EDGE		true

#define MUTUL_NUM_HX83108A (HX83108A_RX_NUM * HX83108A_TX_NUM)
#define SELF_NUM_HX83108A (HX83108A_RX_NUM + HX83108A_TX_NUM)

#if defined(CONFIG_TOUCHSCREEN_HIMAX_ZF_DEBUG)
#define HX_TP_SYS_DIAG
#define HX_TP_SYS_RESET
#define HX_TP_SYS_REGISTER
#define HX_TP_SYS_DEBUG
#define HX_TP_SYS_SELF_TEST

#endif
//===========Himax Option function============
#define HX_ESD_WORKAROUND

#define HX_85XX_A_SERIES_PWON		1
#define HX_85XX_B_SERIES_PWON		2
#define HX_85XX_C_SERIES_PWON		3
#define HX_85XX_D_SERIES_PWON		4
#define HX_85XX_E_SERIES_PWON		5
#define HX_85XX_ES_SERIES_PWON		6
#define HX_85XX_F_SERIES_PWON		7
#define HX_83100A_SERIES_PWON		8
#define HX_83102A_SERIES_PWON		9
#define HX_83102B_SERIES_PWON		10
#define HX_83110A_SERIES_PWON		11
#define HX_83110B_SERIES_PWON		12
#define HX_83111B_SERIES_PWON		13
#define HX_83112A_SERIES_PWON		14
#define HX_83102E_SERIES_PWON		15
#define HX_83108A_SERIES_PWON		16
/* change for hx83108a start*/
#define HX_83108A_ID_PART_1		0x83
#define HX_83108A_ID_PART_2		0x10
#define HX_83108A_ID_PART_3		0x8A
#define HX_83108B_ID_PART_3		0x8B

/* change for hx83108a end*/
#define HX_TP_BIN_CHECKSUM_SW	1
#define HX_TP_BIN_CHECKSUM_HW	2
#define HX_TP_BIN_CHECKSUM_CRC	3

#define HX_KEY_MAX_COUNT		4
#define DEFAULT_RETRY_CNT		3
#define MAX_RETRY_CNT			10

#define ON	1
#define OFF	0

#define HX_VKEY_0		KEY_BACK
#define HX_VKEY_1		KEY_HOME
#define HX_VKEY_2		KEY_RESERVED
#define HX_VKEY_3		KEY_RESERVED
#define HX_KEY_ARRAY	{HX_VKEY_0, HX_VKEY_1, HX_VKEY_2, HX_VKEY_3}

#define SHIFTBITS	5
#define FLASH_SIZE	65536

#define SUSPEND_REJECT	0
#define SUSPEND_IN		0
#define RESUME_IN		0
#define FW_NO_EXIST		0
#define FW_NO_NEED_TO_UPDATE	0
#define FW_NEED_TO_UPDATE		1

#define FW_UPDATE_BOOT	0
#define FW_UPDATE_SD	1

#define FW_SIZE_32k			32768
#define FW_SIZE_60k			61440
#define FW_SIZE_64k			65536
#define FW_SIZE_124k		126976
#define FW_SIZE_128k		131072

#define FACTORY_RUNNING (-1)
#define FACTORY_CANT_RUN (-1)
#define SPI_WORK_ERR (-1)
#define ALLOC_FAIL (-1)
#define NO_RESET_OUT 1
#define LOAD_SENSORCONFIG_OK 1
#define LOAD_SENSORCONFIG_RUN_FAIL 1

#define CAL_CHECKSUM_RUN_FAIL 1
#define MUTUAL_ALLOC_FAIL 1
#define IC_PACK_CHECK_SUCC true
#define IC_PACK_CHECK_FAIL false

#define INFO_FAIL (-1)

#define TP_DCLIENT_NO_EXIST (-1)
#define BUFFER_BUSY (-1)

#define ESD_EVENT_ALL_ZERO		0x00
#define ESD_EVENT_ALL_EB		0xEB
#define ESD_EVENT_ALL_EC		0xEC
#define ESD_EVENT_ALL_ED		0xED
#define ESD_ALL_ZERO_BAK_VALUE	1
#define HX_RB_FRAME_SIZE 40

enum hmx_tp_error_state {
	FWU_GET_SYSINFO_FAIL = 0,
	FWU_GENERATE_FW_NAME_FAIL,
	FWU_REQUEST_FW_FAIL,
	FWU_FW_CRC_ERROR,
	FWU_FW_WRITE_ERROR,
	TP_WOKR_READ_DATA_ERROR = 5,
	TP_WOKR_CHECKSUM_INFO_ERROR,
	TP_WOKR_CHECKSUM_ALL_ERROR,
	TP_WOKR_HEAD_ERROR,
	TS_UPDATE_STATE_UNDEFINE = 255,
};

struct frame_data {
	uint32_t index;
	int16_t *mutual;
	int16_t *self;
	uint32_t cnt_update;
};

struct frame_ring_buf {
	struct frame_data *rawdata;
	uint32_t frame_idx[HX_RB_FRAME_SIZE];
	atomic_t p_update;
	atomic_t p_output;
	atomic_t length;
};

struct zf_info {
	uint8_t sram_addr[4];
	int write_size;
	uint32_t fw_addr;
	uint32_t cfg_addr;
};

struct zf_opt_crc {
	uint32_t fw_addr;
	uint8_t start_addr[4];
	uint8_t start_data[4];
	uint8_t end_addr[4];
	uint8_t end_data[4];
};

struct hmx_dsm_info {
	int irq_gpio;
	int rst_gpio;
	int UPDATE_status;
	int WORK_status;
	int ESD_status;
	int rawdata_status;
};
extern struct hmx_dsm_info hmx_zf_tp_dsm_info;
extern struct dsm_client *hmx_zf_tp_dclient;

struct himax_virtual_key {
	int index;
	int keycode;
	int x_range_min;
	int x_range_max;
	int y_range_min;
	int y_range_max;
};

struct himax_ts_data {

	bool suspended;
	atomic_t suspend_mode;
	uint8_t x_channel;
	uint8_t y_channel;
	uint8_t useScreenRes;//1:display resulotion;0:touch resulotion
	int vendor_fw_ver;
	int vendor_config_ver;
	int vendor_cid_maj_ver;
	int vendor_cid_min_ver;
	int vendor_panel_ver;
	int vendor_sensor_id;
	u32 esd_palm_iron_support;
	uint8_t hx_state_info[2];
	uint8_t power_support;
	uint8_t rst_support;
	uint8_t re_send_cmd_support;
	uint8_t rst_gpio;
	uint8_t first_pressed;
	uint8_t coord_data_size;
	uint8_t area_data_size;
	uint8_t raw_data_frame_size;
	uint8_t raw_data_nframes;
	uint8_t nFinger_support;
	uint8_t irq_enabled;

	struct workqueue_struct *hx_diag_wq;
	struct delayed_work hx_diag_delay_work;

	uint16_t last_slot;
	uint16_t pre_finger_mask;

	uint32_t debug_log_level;
	uint32_t widthFactor;
	uint32_t heightFactor;
	uint32_t tw_x_min;
	uint32_t tw_x_max;
	uint32_t tw_y_min;
	uint32_t tw_y_max;
	uint32_t pl_x_min;
	uint32_t pl_x_max;
	uint32_t pl_y_min;
	uint32_t pl_y_max;
	uint32_t test_capacitance_via_csvfile;
	uint32_t csvfile_use_system;
	uint32_t p2p_test_sel;

	int (*power)(int on);
	int pre_finger_data[10][2];
	char vendor_name[7];
	struct device *dev;
	struct input_dev *input_dev;
	struct himax_platform_data *pdata;
	struct himax_virtual_key *button;
#if defined(CONFIG_FB)
	struct notifier_block fb_notif;
#endif

	char smwp_enable;   //gesture switch

	struct ts_kit_device_data *tskit_himax_data;
	struct platform_device *ts_dev;
//******* raw data *******/
	struct workqueue_struct *himax_wq;
	struct work_struct work;
//******* SPI-start *******/
	struct spi_device	*spi;
//******* SPI-end *******/
	char project_id[MAX_STR_LEN];
	char color_id[HIMAX_TPCOLOR_LEN];
	char module_vendor[MAX_STR_LEN];
	uint8_t support_get_tp_color; /* for tp color */
	uint8_t support_read_projectid;
	uint8_t support_hx_opt_hw_crc;
	bool firmware_updating;
	unsigned char roi_data[ROI_DATA_READ_LENGTH];

	int roiflag;
	uint8_t *hx_rawdata_buf_roi;
	uint8_t *hx_delta_coord;
	uint8_t *hx_delta_coord_roi;
	uint8_t *hx_coord_roi;
	uint8_t *info_data;
	uint8_t *tmp_buffer;
};

struct himax_touching_data {
	int x;
	int y;
	int w;
	uint32_t loop_i;
	uint16_t old_finger;
	uint16_t finger_num;
};

#define HX_CMD_NOP						0x00
#define HX_CMD_SETMICROOFF				0x35
#define HX_CMD_SETROMRDY				0x36
#define HX_CMD_TSSLPIN					0x80
#define HX_CMD_TSSLPOUT					0x81
#define HX_CMD_TSSOFF					0x82
#define HX_CMD_TSSON					0x83
#define HX_CMD_ROE						0x85
#define HX_CMD_RAE						0x86
#define HX_CMD_RLE						0x87
#define HX_CMD_CLRES					0x88
#define HX_CMD_TSSWRESET				0x9E
#define HX_CMD_SETDEEPSTB				0xD7
#define HX_CMD_SET_CACHE_FUN			0xDD
#define HX_CMD_SETIDLE					0xF2
#define HX_CMD_SETIDLEDELAY				0xF3
#define HX_CMD_SELFTEST_BUFFER			0x8D
#define HX_CMD_MANUALMODE				0x42
#define HX_CMD_FLASH_ENABLE				0x43
#define HX_CMD_FLASH_SET_ADDRESS		0x44
#define HX_CMD_FLASH_WRITE_REGISTER	0x45
#define HX_CMD_FLASH_SET_COMMAND		0x47
#define HX_CMD_FLASH_WRITE_BUFFER		0x48
#define HX_CMD_FLASH_PAGE_ERASE			0x4D
#define HX_CMD_FLASH_SECTOR_ERASE		0x4E
#define HX_CMD_CB						0xCB
#define HX_CMD_EA						0xEA
#define HX_CMD_4A						0x4A
#define HX_CMD_4F						0x4F
#define HX_CMD_B9						0xB9
#define HX_CMD_76						0x76
#define HX_CMD_ADDR_RESULT				0X96
#define HX_CMD_ADDR_CRITERIA			0X98
#define HX_VER_FW_MAJ					0x33
#define HX_VER_FW_MIN					0x32
#define HX_VER_FW_CFG					0x39
#define HX_MAX_PRBUF_SIZE				PIPE_BUF

#define HX_FW_DATA_FW_STOP			0x000000A5
#define HX_FW_ADDR_CTRL_FW			0x9000005c
#define HX_FW_ADDR_FLAG_RST_EVENT	0x900000E4
#define HX_ADDR_RELOAD_ACTIVE		0x90000048
#define HX_DATA_RELOAD_ACTIVE		0x000000EC
#define HX_FW_WAKING				0x05

#define HX_ADDR_CRC_S1 0x800B0048
#define HX_ADDR_CRC_S2 0x800B005C
#define HX_ADDR_CRC_S3 0x800B0008
#define HX_ADDR_CRC_S4 0x800B0044
#define HX_DATA_CRC_S1 0x00000030
#define HX_DATA_CRC_S2 0x0000CAAC
#define HX_DATA_CRC_S3 0x00000001
#define HX_DATA_CRC_S4 0x00000000

enum input_protocol_type {
	PROTOCOL_TYPE_A = 0x00,
	PROTOCOL_TYPE_B = 0x01,
};

enum himax_event_id {
	HIMAX_EV_NO_EVENT,
	HIMAX_EV_TOUCHDOWN,
	HIMAX_EV_MOVE,
	HIMAX_EV_LIFTOFF,
};

void hx_zf_sense_on(uint8_t FlashMode);
#if defined(CONFIG_TOUCHSCREEN_HIMAX_ZF_DEBUG)

	extern uint8_t hx_zf_diag_coor[HX_RECEIVE_BUF_MAX_SIZE];

#ifdef HX_TP_SYS_DIAG
	extern int16_t *hx_zf_getMutualBuffer(void);
	extern int16_t *hx_zf_getSelfBuffer(void);
	extern int16_t hx_zf_getDiagCommand(void);
	extern int16_t hx_zf_getXChannel(void);
	extern int16_t hx_zf_getYChannel(void);

	extern void hx_zf_setMutualBuffer(void);
	extern void hx_zf_setMutualNewBuffer(void);
	extern void hx_zf_setMutualOldBuffer(void);
	extern void hx_zf_freeMutualBuffer(void);
	extern void hx_zf_freeMutualNewBuffer(void);
	extern void hx_zf_freeMutualOldBuffer(void);
	extern void hx_zf_freeSelfBuffer(void);
	extern void hx_zf_setXChannel(uint8_t x);
	extern void hx_zf_setYChannel(uint8_t y);
#endif

#endif

extern int HX_ZF_RX_NUM;
extern int HX_ZF_TX_NUM;
extern int HX_ZF_BT_NUM;
extern int HX_ZF_X_RES;
extern int HX_ZF_Y_RES;
extern int HX_ZF_MAX_PT;
extern bool HX_ZF_XY_REVERSE;

extern unsigned long ZF_FW_VER_MAJ_FLASH_ADDR;
extern unsigned long ZF_FW_VER_MAJ_FLASH_LENG;
extern unsigned long ZF_FW_VER_MIN_FLASH_ADDR;
extern unsigned long ZF_FW_VER_MIN_FLASH_LENG;
extern unsigned long ZF_CFG_VER_MAJ_FLASH_ADDR;
extern unsigned long ZF_CFG_VER_MAJ_FLASH_LENG;
extern unsigned long ZF_CFG_VER_MIN_FLASH_ADDR;
extern unsigned long ZF_CFG_VER_MIN_FLASH_LENG;
extern unsigned long ZF_CID_VER_MAJ_FLASH_ADDR;
extern unsigned long ZF_CID_VER_MIN_FLASH_ADDR;
extern unsigned long ZF_CID_VER_MAJ_FLASH_LENG;
extern unsigned long ZF_CID_VER_MIN_FLASH_LENG;
extern uint32_t NC_CFG_TABLE_FLASH_ADDR;

extern unsigned char IC_ZF_CHECKSUM;
extern unsigned char IC_ZF_TYPE;
extern u8 cypress_ts_kit_color[TP_COLOR_SIZE];
extern int zf_self_test_flag;
extern atomic_t hmx_zf_mmi_test_status;
extern struct ts_kit_platform_data g_ts_kit_platform_data;

//for update test
extern unsigned int HX_ZF_UPDATE_FLAG;
extern unsigned int HX_ZF_RESET_COUNT;
//for factory test
extern int g_zf_diag_command;

extern int g_process_type;
extern int g_diag_num_type;
extern int g_process_full;
extern struct frame_ring_buf g_rb_frame;

//define in debug
int hx_zf_hw_reset(bool loadconfig, bool int_off);
void hx_reload_to_active(void);
int hx_reset_device(void);
void crc_data_prepare(const struct firmware *fw);
void himax_zf_read_fw_status(void);
int hx_zf_touch_sysfs_init(void);
void hx_zf_get_dsram_data(uint8_t *info_data);
void hx_zf_diag_register_set(uint8_t diag_command);
void hx_zf_return_event_stack(void);
void hx_zf_idle_mode(int disable);
int hx_zf_switch_mode(int mode);
void himax_zf_read_FW_info(void);
//need to check in detect
extern int hx_zf_fts_ctpm_fw_upgrade_with_fs(unsigned char *fw, int len, bool change_iref);
void hx_zf_get_information(void);
//used in factory test
extern int himax_zf_chip_self_test(void);
int hx_zf_fw_update_boot(char *file_name);
int hx_zf_fw_update_sd(void);
//used in platform
extern void himax_zf_ts_diag_work_func(struct work_struct *work);
void hx_zf_interface_on(void);
int hx_reg_assign(int reg_data,  uint8_t *data_buf);
int hx_addr_reg_assign(int reg_addr, int reg_data, uint8_t *addr_buf, uint8_t *data_buf);

int hx_zf_factory_start(struct himax_ts_data *ts, struct ts_rawdata_info *info_top);
extern int himax_zf_bus_read(uint8_t command, uint8_t *data, uint16_t length, uint16_t limit_len, uint8_t toRetry);
extern void himax_zf_rst_gpio_set(int pinnum, uint8_t value);
extern int himax_zf_write_read_reg(uint8_t *tmp_addr, uint8_t *tmp_data, uint8_t hb, uint8_t lb);
extern void hx_zf_setMutualNewBuffer(void);
extern int16_t *hx_zf_getMutualOldBuffer(void);
extern void hx_zf_setSelfBuffer(void);
extern void himax_zf_int_enable(int irqnum, int enable);
extern int16_t *hx_zf_getMutualNewBuffer(void);
int hx_zf_fw_resume_update(char *file_name);
extern int  firmware_update(const struct firmware *fw);
extern int hostprocessing_get_project_id(char *out, int len);
bool hx_zf_sense_off(void);
uint32_t hx_zf_hw_check_crc(uint8_t *start_addr, int reload_length);
void hx_zf_reload_disable(void);
void hx_resend_command(void);
void hx_zf_diag_node_set(uint8_t diag_command, uint8_t storage_type);
#endif
