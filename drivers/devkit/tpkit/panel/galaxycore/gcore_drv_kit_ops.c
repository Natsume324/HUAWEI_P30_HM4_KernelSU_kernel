/*
 * GalaxyCore touchscreen driver
 *
 * Copyright (C) 2023 GalaxyCore Incorporated
 *
 * Copyright (C) 2023 Newgate tian <newgate_tian@gcoreinc.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */
#include "gcore_drv_common.h"
#ifdef CONFIG_BOOT_UPDATE_FIRMWARE_BY_ARRAY
#include "gcore_drv_firmware.h"
#endif
#include <linux/hrtimer.h>
#include <linux/cpu.h>
struct gcore_dev *gdev_kit = NULL;
unsigned char gcore_roi_data[ROI_DATA_READ_LENGTH] = { 0 };

#define PINCTRL_STATE_RESET_HIGH "state_rst_output1"
#define PINCTRL_STATE_RESET_LOW "state_rst_output0"
#define PINCTRL_INT_STATE_HIGH "state_eint_output1"
#define PINCTRL_INT_STATE_LOW "state_eint_output0"
#define PINCTRL_STATE_AS_INT "state_eint_as_int"
#define GTP_TRANS_X(x, tpd_x_res, lcm_x_res) (((x) * (lcm_x_res)) / (tpd_x_res))
#define GTP_TRANS_Y(y, tpd_y_res, lcm_y_res) (((y) * (lcm_y_res)) / (tpd_y_res))
#define STARTUP_UPDATE_DELAY_BOOT_MS 1500
#define TEN_FINGER_TOUCH_SCREEN 10
#define PULL_UP 1
#define PULL_DOWM 0
#define GCORE_MAX 255
#define GCORE_MIN 0
#define ID_OFFSET 1

#if defined(CONFIG_HUAWEI_DEVKIT_HISI)
extern int hostprocessing_get_project_id(char *out, int len);
#endif

#ifdef CONFIG_DETECT_FW_RESTART_INIT_EN
static u8 fw_init_values[] = { 0xB3, 0xFF, 0xFF, 0x33, 0x28, 0x04, 0x01, 0xFF, 0xFF };
static int fw_init_count = 0;
static int fw_init_notcount_flag = -1;
static u8 fw_status = 0;

int get_fw_init_count(void)
{
	return fw_init_count;
}
void set_fw_init_count(int count)
{
	fw_init_count = count;
}
int get_fw_init_notcount_flag(void)
{
	return fw_init_notcount_flag;
}
void set_fw_init_notcount_flag(int flag)
{
	GTP_DEBUG("flag:%d", flag);
	fw_init_notcount_flag = flag;
}
u8 get_fw_status(void)
{
	GTP_DEBUG("%x", fw_status);
	return fw_status;
}

struct hrtimer gcore_timer;
static enum hrtimer_restart gcore_timer_handler(struct hrtimer *timer)
{
	set_fw_init_notcount_flag(0);
	return HRTIMER_NORESTART;
}

void fw_init_count_timer_init(struct gcore_dev *gdev)
{
	hrtimer_init(&gcore_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	gcore_timer.function = gcore_timer_handler;
	set_fw_init_notcount_flag(0);
	GTP_DEBUG("gcore_timer init success!");
}
void fw_init_count_timer_start(int flag)
{
	if (get_fw_init_notcount_flag() >= 0) {
		set_fw_init_notcount_flag(flag);
		hrtimer_start(&gcore_timer, ms_to_ktime(10), HRTIMER_MODE_REL);
	} else {
		GTP_DEBUG("gcore_timer null, not init yet!");
	}
}
void fw_init_count_timer_exit(void)
{
	hrtimer_cancel(&gcore_timer);
}

#endif /* CONFIG_DETECT_FW_RESTART_INIT_EN */

static int gcore_pinctrl_init(void)
{
	int error = 0;
	gdev_kit->pctrl = devm_pinctrl_get(&gdev_kit->ts_dev->dev);
	if (IS_ERR_OR_NULL(gdev_kit->pctrl)) {
		GTP_ERROR("failed to devm pinctrl get\n");
		return -EINVAL;
	}

	gdev_kit->pins_default = pinctrl_lookup_state(gdev_kit->pctrl, "default");
	if (IS_ERR(gdev_kit->pins_default)) {
		GTP_ERROR("failed to pinctrl lookup state default.\n");
		error = -EINVAL;
		goto err_pinctrl_put;
	}

	gdev_kit->pins_idle = pinctrl_lookup_state(gdev_kit->pctrl, "idle");
	if (IS_ERR(gdev_kit->pins_idle)) {
		GTP_ERROR("failed to pinctrl lookup state idle");
		error = -EINVAL;
		goto err_pinctrl_put;
	}

	gdev_kit->pinctrl_state_reset_high =
		pinctrl_lookup_state(gdev_kit->pctrl, PINCTRL_STATE_RESET_HIGH);
	if (IS_ERR_OR_NULL(gdev_kit->pinctrl_state_reset_high)) {
		GTP_ERROR("Can not lookup %s pinstate", PINCTRL_STATE_RESET_HIGH);
		error = -EINVAL;
		goto err_pinctrl_put;
	}

	gdev_kit->pinctrl_state_reset_low =
		pinctrl_lookup_state(gdev_kit->pctrl, PINCTRL_STATE_RESET_LOW);
	if (IS_ERR_OR_NULL(gdev_kit->pinctrl_state_reset_low)) {
		GTP_ERROR("Can not lookup %s pinstate", PINCTRL_STATE_RESET_LOW);
		error = -EINVAL;
		goto err_pinctrl_put;
	}

	gdev_kit->pinctrl_state_as_int =
		pinctrl_lookup_state(gdev_kit->pctrl, PINCTRL_STATE_AS_INT);
	if (IS_ERR_OR_NULL(gdev_kit->pinctrl_state_as_int)) {
		GTP_ERROR("Can not lookup %s pinstate", PINCTRL_STATE_AS_INT);
		error = -EINVAL;
		goto err_pinctrl_put;
	}
	error = pinctrl_select_state(gdev_kit->pctrl, gdev_kit->pinctrl_state_as_int);
	if (error < 0) {
		GTP_ERROR("set gpio as int failed\n");
		error = -EINVAL;
		goto err_pinctrl_put;
	}
	return 0;
err_pinctrl_put:
	devm_pinctrl_put(gdev_kit->pctrl);
	return error;
}
void gcore_kit_parse_config(void)
{
	int ret;
	u32 read_val = 0;
	struct device_node *device = NULL;
	struct ts_kit_device_data *chip_data = NULL;
	const char *chipname = NULL;
	const char *tptesttype = NULL;
	struct ts_roi_info *roi_info = NULL;
	u8 dts_ic_type;
	struct ts_horizon_info *horizon_info = NULL;
	chip_data = gdev_kit->tskit_data->ts_platform_data->chip_data;
	horizon_info = &gdev_kit->tskit_data->ts_platform_data->feature_info.horizon_info;
	device = gdev_kit->ts_dev->dev.of_node;
	GTP_DEBUG("parse_dts: parameter init begin");
	if (NULL == device || NULL == chip_data)
		return;
	ret = of_property_read_u32(device, "irq_config", &chip_data->irq_config);
	if (ret) {
		GTP_ERROR("Not define irq_config in Dts");
		chip_data->irq_config = 3;
	}
	GTP_DEBUG("get irq_config = %d", chip_data->irq_config);
	ret = of_property_read_u32(device, "ic_type", &dts_ic_type);
	if (ret)
		GTP_ERROR("Not define irq_type in Dts");
	gdev_kit->gc_ic_type = dts_ic_type;
	GTP_DEBUG("get ic_type = %d", gdev_kit->gc_ic_type);
	ret = of_property_read_string(device, "chip_name", &chipname);
	if (ret) {
		GTP_ERROR("Not define module in Dts!");
	} else {
		strncpy(chip_data->chip_name, chipname, GTP_CHIP_NAME_LEN);
	}
	GTP_DEBUG("get gcore_chipname = %s", chip_data->chip_name);
	ret = of_property_read_u32(device, "horizon_config", &read_val);
	if (!ret) {
		horizon_info->horizon_supported = (u8)read_val;
		GTP_DEBUG("Set horizon_supported %d", horizon_info->horizon_supported);
	} else {
		GTP_DEBUG("horizon_supported default");
	}
	ret = of_property_read_string(device, "tp_test_type", &tptesttype);
	if (ret) {
		GTP_DEBUG("Not define device tp_test_type in Dts, use default");
		strncpy(chip_data->tp_test_type, "Normalize_type:judge_last_result",
			GTP_TS_CAP_TEST_TYPE_LEN);
	} else {
		snprintf(chip_data->tp_test_type, GTP_TS_CAP_TEST_TYPE_LEN, "%s",
			tptesttype);
	}
	roi_info = &gdev_kit->tskit_data->ts_platform_data->feature_info.roi_info;
	ret = of_property_read_u32(device, "roi_supported", &read_val);
	if (ret) {
		GTP_ERROR("Not define roi_supported in Dts, use default");
		roi_info->roi_supported = 1;
	} else {
		roi_info->roi_supported = (u8)read_val;
	}
#ifdef CONFIG_GCORE_KIT_PSY_NOTIFY
	chip_data->ts_platform_data->feature_info.charger_info.charger_supported = 1;
	GTP_DEBUG("charger_support = %d", chip_data->ts_platform_data->feature_info
										  .charger_info.charger_supported);
#endif
}


static int gcore_chip_detect(struct ts_kit_platform_data *data)
{
	int ret = NO_ERR;

	if (data == NULL) {
		GTP_ERROR("Detect chip with ts_kit_platform_data = NULL");
		return -EINVAL;
	}

	if (gdev_kit == NULL) {
		GTP_ERROR("Detect chip with gdev_kit = NULL");
		return -EINVAL;
	}

	GTP_DEBUG("gcore Chip detect");
	gdev_kit->platformkit_data = data;
	gdev_kit->tskit_data->ts_platform_data = data;
	gdev_kit->ts_dev = data->ts_dev;
	gdev_kit->ts_dev->dev.of_node = gdev_kit->tskit_data->cnode;
	gcore_kit_parse_config();

	/* 1: enable download firmware in recovery mode */
	data->chip_data->download_fw_inrecovery = 1;

#ifdef CONFIG_HUAWEI_TS_KIT
	gdev_kit->irq_gpio = data->irq_gpio;
	gdev_kit->rst_gpio = data->reset_gpio;
#else
	ret = gcore_pinctrl_init();
	if (ret) {
		GTP_ERROR("chip_detect: pinctrl init failed");
		return -EINVAL;
	}
#endif
#ifdef CONFIG_TOUCH_DRIVER_INTERFACE_SPI
	ret = gcore_spi_probe(gdev_kit->platformkit_data->spi);
#endif

	return ret;
}

static int read_projectid_from_lcd(void)
{
	int retval;
	retval = hostprocessing_get_project_id(gdev_kit->tskit_data->project_id,
		GTP_CHIP_NAME_LEN);
	if (retval)
		GTP_ERROR("%s, fail get project_id", __func__);
	GTP_DEBUG("kit_project_id=%s", gdev_kit->tskit_data->project_id);
	return retval;
}

static int gcore_kit_chip_init(void)
{
	read_projectid_from_lcd();
	if (gcore_touch_probe(gdev_kit)) {
		GTP_ERROR("touch registration fail!");
		return -EPERM;
	}
	return 0;
}

static int gcore_kit_input_config(struct input_dev *input_dev)
{
	if (gdev_kit == NULL) {
		GTP_ERROR("Config input device with gdev_kit = NULL");
		return -EFAULT;
	}

	if (input_dev == NULL) {
		GTP_ERROR("Config input device with input_dev = NULL");
		return -EINVAL;
	}
	unsigned int tpd_res_x;
	unsigned int tpd_res_y;
#ifdef CONFIG_ENABLE_TYPE_B_PROCOTOL
	unsigned int tpd_max_fingers;
#endif
	gdev_kit->tskit_data->x_max = TOUCH_SCREEN_X_MAX;
	gdev_kit->tskit_data->y_max = TOUCH_SCREEN_Y_MAX;
	GTP_DEBUG("input: X: %u, Y: %u", gdev_kit->tskit_data->x_max, gdev_kit->tskit_data->y_max);
	tpd_res_x = gdev_kit->tskit_data->x_max;
	tpd_res_y = gdev_kit->tskit_data->y_max;
	gdev_kit->input_device = input_dev;
	set_bit(EV_ABS, gdev_kit->input_device->evbit);
	set_bit(EV_KEY, gdev_kit->input_device->evbit);
	set_bit(EV_SYN, gdev_kit->input_device->evbit);
	set_bit(INPUT_PROP_DIRECT, gdev_kit->input_device->propbit);

	set_bit(BTN_TOUCH, gdev_kit->input_device->keybit);
	set_bit(TS_DOUBLE_CLICK, gdev_kit->input_device->keybit);
	input_set_abs_params(gdev_kit->input_device, ABS_MT_POSITION_X, 0, tpd_res_x, 0, 0);
	input_set_abs_params(gdev_kit->input_device, ABS_MT_POSITION_Y, 0, tpd_res_y, 0, 0);
	input_set_abs_params(gdev_kit->input_device, ABS_MT_TRACKING_ID,
		0, TEN_FINGER_TOUCH_SCREEN, 0, 0);
	input_set_abs_params(gdev_kit->input_device, ABS_MT_TOUCH_MAJOR, GCORE_MIN,
		GCORE_MAX, GCORE_MIN, GCORE_MIN);
	input_set_abs_params(gdev_kit->input_device, ABS_MT_PRESSURE, GCORE_MIN,
		GCORE_MAX, GCORE_MIN, GCORE_MIN);
	input_set_abs_params(gdev_kit->input_device, ABS_MT_WIDTH_MAJOR, GCORE_MIN,
		GCORE_MAX, GCORE_MIN, GCORE_MIN);
#ifdef CONFIG_ENABLE_GESTURE_WAKEUP
	set_bit(GESTURE_KEY, gdev_kit->input_device->keybit);
#endif

#ifdef CONFIG_ENABLE_TYPE_B_PROCOTOL
	tpd_max_fingers = gdev_kit->tskit_data->ts_platform_data->max_fingers;
	if (input_mt_init_slots(gdev_kit->input_device, tpd_max_fingers,
		INPUT_MT_DIRECT))
		GTP_ERROR("mt slot init fail");
#else
	/* for single-touch device */
	set_bit(BTN_TOUCH, gdev_kit->input_device->keybit);
#endif

	return 0;
}


void gcore_kit_touch_down(struct ts_fingers *info, s32 x, s32 y, u8 id)
{
	GTP_REPORT("touch down");
	info->fingers[id].x = x;
	info->fingers[id].y = y;
	info->fingers[id].major = 10;
	info->fingers[id].minor = 10;
	info->fingers[id].pressure = 30;
	info->fingers[id].status = TS_FINGER_PRESS;
	info->cur_finger_number = id + ID_OFFSET;
}

void gcore_kit_touch_up(struct ts_fingers *info, u8 id)
{
	GTP_REPORT("touch up");
	info->fingers[id].status = TS_FINGER_RELEASE;
	info->cur_finger_number = id;
}

#if GCORE_WDT_RECOVERY_ENABLE
static u8 wdt_valid_values[] = { 0xFF, 0xFF, 0x72, 0x02, 0x00, 0x01, 0xFF, 0xFF };
#endif
#ifdef CONFIG_SAVE_CB_CHECK_VALUE
static u8 CB_check_value[] = { 0xFF, 0xFF, 0xCB, 0x00, 0x00, 0xCB, 0xFF, 0xFF };
static u8 fw_rcCB_value[] = { 0xFF, 0xFF, 0x00, 0xCB, 0xCB, 0x00, 0xFF, 0xFF };
#endif

s32 gcore_kit_touch_event_report(struct gcore_dev *gdev,
	struct ts_fingers *info, struct ts_cmd_node *out_cmd)
{
	u8 *coor_data = NULL;
	s32 i = 0;
	u8 id = 0;
	u8 status = 0;
	bool touch_end = true;
	u8 checksum = 0;
	u16 touchdata = 0;
#ifdef CONFIG_ENABLE_TYPE_B_PROCOTOL
	static unsigned long prev_touch;
#endif
	unsigned long curr_touch = 0;
	s32 input_x = 0;
	s32 input_y = 0;
	int data_size = DEMO_DATA_SIZE;

	unsigned int tpd_res_x = gdev_kit->tskit_data->x_max;
	unsigned int tpd_res_y = gdev_kit->tskit_data->y_max;
	unsigned int lcm_res_x = gdev_kit->tskit_data->x_max;
	unsigned int lcm_res_y = gdev_kit->tskit_data->y_max;

#ifdef CONFIG_ENABLE_FW_RAWDATA
	if (gdev->fw_mode == DEMO) {
		if (gdev->gtp_roi_switch)
			data_size = DEMO_DATA_SIZE+ROI_DATA_READ_LENGTH;
		else
			data_size = DEMO_DATA_SIZE;
	} else if (gdev->fw_mode == RAWDATA) {
		data_size = g_rawdata_row * g_rawdata_col * 2;
	} else if (gdev->fw_mode == DEMO_RAWDATA) {
#ifdef CONFIG_TOUCH_DRIVER_INTERFACE_SPI
		data_size = 2048; /* mtk platform spi transfer len request */
#else
		data_size = DEMO_DATA_SIZE + (g_rawdata_row * g_rawdata_col * 2);
#endif
	} else if (gdev->fw_mode == DEMO_RAWDATA_DEBUG) {
#ifdef CONFIG_TOUCH_DRIVER_INTERFACE_SPI
		data_size = 2048; /* mtk platform spi transfer len request */
#else
		data_size =
			DEMO_DATA_SIZE + ((g_rawdata_row + 1) * (g_rawdata_col + 1) * 2);
#endif
	} else if (gdev->fw_mode == FW_DEBUG) {
		data_size = 175;
	} else {
#ifdef CONFIG_TOUCH_DRIVER_INTERFACE_SPI
		data_size = 2048; /* mtk platform spi transfer len request */
#else
		data_size = DEMO_DATA_SIZE + (g_rawdata_row * g_rawdata_col * 2);
#endif
	}
#endif
	if (data_size > DEMO_RAWDATA_MAX_SIZE) {
		GTP_ERROR("touch data read length exceed MAX_SIZE");
		return -EPERM;
	}
	GTP_REPORT("data size = %d", data_size);
	if (gcore_bus_read(gdev->touch_data, data_size)) {
		GTP_ERROR("touch data read error");
		return -EPERM;
	}

#ifdef CONFIG_ENABLE_FW_RAWDATA
	if (gdev->usr_read) {
		gdev->data_ready = true;
		wake_up_interruptible(&gdev->usr_wait);

		if (gdev->fw_mode == RAWDATA)
			return 0;
	}
#endif

	if (gdev->fw_mode == DEMO_RAWDATA_DEBUG) {
		for (i = 1152 + 65; i < 1319; i += 2) {
			touchdata = (gdev->touch_data[i + 1] << 8) | gdev->touch_data[i];
			printk("<tian> touch_data = "
				   "%d,touch_data[%d+1]=%d,touch_data[%d]=%d\n",
				touchdata, i, gdev->touch_data[i + 1], i, gdev->touch_data[i]);
		}
	}

	coor_data = &gdev->touch_data[0];

	if (demolog_enable) {
		GTP_DEBUG("demolog dbg is on!");
#if GCORE_MP_TEST_ON
		dump_demo_data_to_csv_file(coor_data, 10, 6);
#endif
	}

#ifdef CONFIG_SAVE_CB_CHECK_VALUE
	if (!memcmp(coor_data, CB_check_value, sizeof(CB_check_value))) {
		gdev->CB_ckstat = true;
		gdev->cb_count = 0;
		GTP_DEBUG("Start receive and save CB value");
		if (gcore_bus_read(gdev->touch_data, CB_SIZE)) {
			GTP_ERROR("touch data read error");
			return -EPERM;
		}
		gdev->touch_data += 8;
		if (gdev->CB_value == NULL) {
			GTP_ERROR("CB_value is NULL");
			return 0;
		}
		memcpy(gdev->CB_value, gdev->touch_data, CB_SIZE);
		GTP_DEBUG("CB_value[0]: %d,CB_value[1]: %d", gdev->CB_value[0],
			gdev->CB_value[1]);
		gdev->CB_value[0] = 0xCB;
		gdev->CB_value[1] = 0x01;
		return 0; /* receive and save CB value */
	}
	/* When TP resumed,Checks whether the fw has received CB value */
	if (!memcmp(coor_data, fw_rcCB_value, sizeof(fw_rcCB_value))) {
		gdev->CB_ckstat = true;
		/* gdev->CB_dl = false; */
		gdev->cb_count = 0;
		GTP_DEBUG("CB value download complete interrupt detected");
		return 0; /* FW had received CB value */
	}
#endif

#ifdef CONFIG_ENABLE_PROXIMITY_TP_SCREEN_OFF
	if (gdev->PS_Enale == true) {
		if (gdev->TEL_State) {
			if (check_notify_event(coor_data[62], gdev->fw_event)) {
				GTP_DEBUG("TEL:Notify fw event interrupt detected");
				mod_delayed_work(gdev->fwu_workqueue, &gdev->event_work,
					msecs_to_jiffies(36));
				notify_contin = 0;
				gdev->notify_state = false;
				gdev->TEL_State = false;
			}
		}

		if (coor_data[62] & 0x01) {
			GTP_DEBUG("TEL: On The Phone Now");
			if (coor_data[62] == 0x81) {
				gdev->tel_screen_off = true;
				GTP_DEBUG("TEL: Proximity tp turn off Screen,data[62]=0x%x",
					coor_data[62]);
#ifdef CONFIG_MTK_PROXIMITY_TP_SCREEN_ON_ALSPS
				gcore_tpd_proximity_flag_off = true;
				ps_report_interrupt_data(10);

#else
				input_report_abs(gdev->input_ps_device, ABS_DISTANCE, 1);
				input_sync(gdev->input_ps_device);
#endif
			} else {
				gdev->tel_screen_off = false;
				GTP_DEBUG("TEL: Keep Screen On!data[62]=0x%x", coor_data[62]);
#ifdef CONFIG_MTK_PROXIMITY_TP_SCREEN_ON_ALSPS
				gcore_tpd_proximity_flag_off = false;
				ps_report_interrupt_data(0);
#else
				input_report_abs(gdev->input_ps_device, ABS_DISTANCE, 0);
				input_sync(gdev->input_ps_device);
#endif
			}
		}
	}

#endif
	if (gdev->notify_state) {
		if (check_notify_event(coor_data[63], gdev->fw_event)) {
			GTP_DEBUG("Notify fw event interrupt detected");
			mod_delayed_work(gdev->fwu_workqueue, &gdev->event_work,
				msecs_to_jiffies(36));
			notify_contin = 0;
			gdev->notify_state = false;
		}
	}

#if GCORE_WDT_RECOVERY_ENABLE
	if (!gdev->ts_stat) {
		if (!memcmp(coor_data, wdt_valid_values, sizeof(wdt_valid_values))) {
			GTP_REPORT("WDT interrupt detected");
			mod_delayed_work(gdev->fwu_workqueue, &gdev->wdt_work,
				msecs_to_jiffies(GCORE_WDT_TIMEOUT_PERIOD));
			wdt_contin = 0;
			return 0; /* WDT interrupt detected */
		}
	}
#endif

#ifdef CONFIG_DETECT_FW_RESTART_INIT_EN
	if (!memcmp(coor_data, fw_init_values, sizeof(fw_init_values))) {
		if (get_fw_init_notcount_flag() == 0) {
			fw_init_count++;
		} else {
			set_fw_init_notcount_flag(0);
		}
		GTP_DEBUG("fw init. count:%d", fw_init_count);
		return 0;
	}
	fw_status = coor_data[60];
#endif

#if defined(CONFIG_GCORE_AUTO_UPDATE_FW_HOSTDOWNLOAD) && \
	defined(CONFIG_GCORE_HOSTDOWNLOAD_ESD_PROTECT)
	/* firmware watchdog, rehostdownload */
	if (gcore_hostdownload_esd_protect(gdev))
		return 0;
#endif

	checksum = Cal8bitsChecksum(coor_data, DEMO_DATA_SIZE - 1);
	if (checksum != coor_data[DEMO_DATA_SIZE - 1]) {
		GTP_ERROR("checksum error! read:%x cal:%x",
			coor_data[DEMO_DATA_SIZE - 1], checksum);
#if 1
		for (i = 1; i <= 10; i++)
			GTP_DEBUG("demo %d:%x %x %x %x %x %x", i, coor_data[6 * i - 6],
				coor_data[6 * i - 5], coor_data[6 * i - 4],
				coor_data[6 * i - 3], coor_data[6 * i - 2],
				coor_data[6 * i - 1]);
		GTP_DEBUG("demo:%x %x %x %x", coor_data[60], coor_data[61],
			coor_data[62], coor_data[63]);
#endif
		return -EPERM;
	}

#ifdef CONFIG_ENABLE_GESTURE_WAKEUP
	gcore_gesture_event_handler(gdev->input_device, (coor_data[61] >> 3));
#endif
	out_cmd->command = TS_INPUT_ALGO;
#if GCORE_WDT_RECOVERY_ENABLE
	if (!gdev->ts_stat) {
		mod_delayed_work(gdev->fwu_workqueue, &gdev->wdt_work,
			msecs_to_jiffies(GCORE_WDT_TIMEOUT_PERIOD));
		wdt_contin = 0;
	}
#endif

	for (i = 1; i <= GTP_MAX_TOUCH; i++) {
		GTP_REPORT("i:%d data:%x %x %x %x %x %x", i, coor_data[0], coor_data[1],
			coor_data[2], coor_data[3], coor_data[4], coor_data[5]);
		if ((coor_data[0] == 0xFF) || (coor_data[0] == 0x00)) {
			coor_data += 6;
			continue;
		}

		touch_end = false;
		id = coor_data[0] >> 3;
		status = coor_data[0] & 0x03;
		input_x = ((coor_data[1] << 4) | (coor_data[3] >> 4));
		input_y = ((coor_data[2] << 4) | (coor_data[3] & 0x0F));

		GTP_REPORT("id:%d (x=%d,y=%d)", id, input_x, input_y);
		GTP_REPORT("tpd_x=%d tpd_y=%d lcm_x=%d lcm_y=%d", tpd_res_x, tpd_res_y,
			lcm_res_x, lcm_res_y);
		if (id > GTP_MAX_TOUCH) {
			coor_data += 6;
			continue;
		}

		input_x = GTP_TRANS_X(input_x, tpd_res_x, lcm_res_x);
		input_y = GTP_TRANS_Y(input_y, tpd_res_y, lcm_res_y);

		set_bit(id, &curr_touch);

		gcore_kit_touch_down(info, input_x, input_y, id - ID_OFFSET);

		coor_data += 6;
	}

#ifdef CONFIG_ENABLE_TYPE_B_PROCOTOL
	for (i = 1; i <= GTP_MAX_TOUCH; i++) {
		if (!test_bit(i, &curr_touch) && test_bit(i, &prev_touch))
			gcore_kit_touch_up(info, i - 1);

		if (test_bit(i, &curr_touch)) {
			set_bit(i, &prev_touch);
		} else {
			clear_bit(i, &prev_touch);
		}

		clear_bit(i, &curr_touch);
	}

#else
	if (touch_end) {
#ifdef ROI
		if (gdev->gtp_roi_switch) {
			memcpy(gcore_roi_data, gdev->touch_data + DEMO_DATA_SIZE, ROI_DATA_READ_LENGTH);
			GTP_REPORT("Get roi data,gcore_roi_data[0] = %d,gcore_roi_data[1] = %d",
				gcore_roi_data[0], gcore_roi_data[1]);
		}
#endif
		gcore_kit_touch_up(info, 0);
	}
#endif

	return 0;
}


int gcore_chip_irq_top_half(struct ts_cmd_node *cmd)
{
	struct gcore_exp_fn *exp_fn = NULL;
	struct gcore_exp_fn *exp_fn_temp = NULL;
	u8 found = 0;
	fn_data.gdev->tpd_flag = 0;
	if (!list_empty(&fn_data.list)) {
		list_for_each_entry_safe(exp_fn, exp_fn_temp, &fn_data.list, link)
		{
			if (exp_fn->wait_int == true) {
				exp_fn->event_flag = true;
				found = 1;
				break;
			}
		}
	}

	if (!found) {
		cmd->command = TS_INT_PROCESS;
	} else {
		wake_up_interruptible(&fn_data.gdev->wait);
		cmd->command = TS_INT_PROCESS;
		fn_data.gdev->tpd_flag = 1;
	}
	return 0;
}
int gcore_chip_irq_bottom_half(struct ts_cmd_node *in_cmd,
	struct ts_cmd_node *out_cmd)
{
	struct ts_fingers *info = NULL;
	if (fn_data.gdev->tpd_flag == 1) {
		fn_data.gdev->tpd_flag = 0;
		return 0;
	}

	if (out_cmd == NULL) {
		GTP_ERROR("Irq bottom half with out_cmd = NULL");
		return -EINVAL;
	}
	// out_cmd->command = TS_INPUT_ALGO;//触发 ts_algo_calibrate 函数
	out_cmd->cmd_param.pub_params.algo_param.algo_order =
		gdev_kit->tskit_data->algo_id;
	info = &out_cmd->cmd_param.pub_params.algo_param.info;
	memset(info, 0, sizeof(*info));

	if (mutex_is_locked(&fn_data.gdev->transfer_lock)) {
		GTP_DEBUG("touch is locked, ignore");
		return -EINVAL;
	}

	mutex_lock(&fn_data.gdev->transfer_lock);
	/* don't reset before "if (tpd_halt.."  */
	GTP_REPORT("tpd_event_handler enter");

	if (gcore_kit_touch_event_report(fn_data.gdev, info, out_cmd))
		GTP_ERROR("touch event handler error");

	mutex_unlock(&fn_data.gdev->transfer_lock);

	return 0;
}

static int gcore_kit_resume(void)
{
	GTP_DEBUG("gcore kit resume start..");
	gpio_direction_output(gdev_kit->tskit_data->ts_platform_data->reset_gpio, PULL_UP);
	gpio_direction_output(gdev_kit->tskit_data->ts_platform_data->cs_gpio, PULL_UP);

#ifdef CONFIG_ENABLE_PROXIMITY_TP_SCREEN_OFF
	if (fn_data.gdev->PS_Enale == true)
		tpd_enable_ps(1);

#endif

#ifdef CONFIG_ENABLE_GESTURE_WAKEUP
	if (fn_data.gdev->gesture_wakeup_en)
		disable_irq_wake(fn_data.gdev->touch_irq);
#endif

#ifdef CONFIG_GCORE_AUTO_UPDATE_FW_HOSTDOWNLOAD
	queue_delayed_work(gdev_kit->fwu_workqueue, &gdev_kit->fwu_work,
		msecs_to_jiffies(100));
#else
#if CONFIG_GCORE_RESUME_EVENT_NOTIFY
	queue_delayed_work(fn_data.gdev->gtp_workqueue,
		&fn_data.gdev->resume_notify_work, msecs_to_jiffies(300));
#endif
#endif
	fn_data.gdev->ts_stat = TS_NORMAL;

	GTP_DEBUG("gcore kit resume end");
	return 0;
}

static int gcore_kit_suspend(void)
{
	GTP_DEBUG("gcore kit suspend start..");
	gpio_direction_output(gdev_kit->tskit_data->ts_platform_data->cs_gpio, PULL_DOWM);
	gpio_direction_output(gdev_kit->tskit_data->ts_platform_data->reset_gpio, PULL_DOWM);

#ifdef CONFIG_ENABLE_PROXIMITY_TP_SCREEN_OFF
	if (fn_data.gdev->PS_Enale == true) {
		GTP_DEBUG("Proximity TP Now");
		return;
	}

#endif

#if GCORE_WDT_RECOVERY_ENABLE
	cancel_delayed_work_sync(&fn_data.gdev->wdt_work);
#endif
	cancel_delayed_work_sync(&fn_data.gdev->fwu_work);

#ifdef CONFIG_ENABLE_GESTURE_WAKEUP
	if (fn_data.gdev->gesture_wakeup_en)
		enable_irq_wake(fn_data.gdev->touch_irq);
#endif
#ifdef CONFIG_SAVE_CB_CHECK_VALUE
	fn_data.gdev->CB_ckstat = false;
#endif

	fn_data.gdev->ts_stat = TS_SUSPEND;

	GTP_DEBUG("gcore kit suspend end");
	return 0;
}

static int gcore_charger_switch(struct ts_charger_info *info)
{
	GTP_DEBUG("charger_switch: called");
	if (info == NULL) {
		GTP_ERROR("charger_switch: pointer info is NULL");
		return -ENOMEM;
	}
	if (info->charger_switch == USB_PIUG_OUT) { /* usb plug out */
		GTP_DEBUG("charger_switch: usb plug out DETECTED");
		if (gdev_kit->ts_stat == TS_SUSPEND) {
			gcore_modify_fw_event_cmd(FW_CHARGER_UNPLUG);
		} else {
			gcore_fw_event_notify(FW_CHARGER_UNPLUG);
		}
	} else if (info->charger_switch == USB_PIUG_IN) { /* usb plug in */
		GTP_DEBUG("charger_switch: usb plug in DETECTED");
		if (gdev_kit->ts_stat == TS_SUSPEND) {
			gcore_modify_fw_event_cmd(FW_CHARGER_PLUG);
		} else {
			gcore_fw_event_notify(FW_CHARGER_PLUG);
		}
	} else {
		GTP_ERROR(
			"charger_switch: unknown USB status, info->charger_switch = %d",
			info->charger_switch);
	}

	return NO_ERR;
}

#define EDGE_0     0
#define EDGE_90    1
#define EDGE_270   3
#define BASE_HORIZON   5
static int gcore_horizon_notify(struct ts_horizon_info *info)
{
	GTP_DEBUG("Notify edge event");
	if (info == NULL) {
		GTP_ERROR("poniter info if NULL");
		return -ENOMEM;
	}

	if (!info->horizon_supported) {
		GTP_DEBUG("horizon change is not supported");
		return 0;
	}

	if ((info->horizon_switch == EDGE_0) || (info->horizon_switch == EDGE_90) ||
		(info->horizon_switch == EDGE_270)) {
		info->horizon_switch += BASE_HORIZON;
		gcore_fw_event_notify(info->horizon_switch);
	} else {
		GTP_DEBUG("No defined value was recognized");
	}

	return NO_ERR;
}

static int gcore_chip_reset(void)
{
	if (gdev_kit == NULL) {
		GTP_ERROR("Chipe reset with gdev_kit = NULL");
		return -EINVAL;
	}

	GTP_DEBUG("Chip reset");
	gcore_reset();

	return 0;
}
#ifdef CONFIG_BOOT_UPDATE_FIRMWARE_BY_ARRAY
u8 g_ret_update_boot;
u8 retry_count_boot = 0;
static u8 *fw_update_buff_boot = NULL;
void gcore_kit_request_firmware_update_boot_work(struct work_struct *work)
{
	fw_update_buff_boot = gcore_default_FW;
	g_ret_update_boot = 0;
	if (IS_ERR_OR_NULL(fw_update_buff))
		fw_update_buff = kzalloc(FW_SIZE, GFP_KERNEL);

	if (IS_ERR_OR_NULL(fw_update_buff))
		GTP_ERROR("fw buf mem allocate fail");

	gdev_kit->fw_mem = fw_update_buff;
	gdev_kit->fw_update_state = true;
	GTP_DEBUG("Start use Array update!");
	if (gcore_auto_update_hostdownload_proc(fw_update_buff_boot)) {
		GTP_ERROR("auto update hostdownload proc fail");
		goto retry;
	}
	gdev_kit->fw_ver_in_bin[0] = fw_update_buff_boot[FW_VERSION_ADDR];
	gdev_kit->fw_ver_in_bin[1] = fw_update_buff_boot[FW_VERSION_ADDR + 1];
	gdev_kit->fw_ver_in_bin[2] = fw_update_buff_boot[FW_VERSION_ADDR + 2];
	gdev_kit->fw_ver_in_bin[3] = fw_update_buff_boot[FW_VERSION_ADDR + 3];
	GTP_DEBUG("fw bin ver:%x %x %x %x", gdev_kit->fw_ver_in_bin[1],
		gdev_kit->fw_ver_in_bin[0], gdev_kit->fw_ver_in_bin[3],
		gdev_kit->fw_ver_in_bin[2]);

#if GCORE_WDT_RECOVERY_ENABLE
	mod_delayed_work(gdev_kit->fwu_workqueue, &gdev_kit->wdt_work,
		msecs_to_jiffies(GCORE_WDT_TIMEOUT_PERIOD));
	gcore_tp_esd_fail = false;
#endif
#ifdef CONFIG_SAVE_CB_CHECK_VALUE
	queue_delayed_work(fn_data.gdev->fwu_workqueue, &fn_data.gdev->cb_work,
		msecs_to_jiffies(20));
#endif

#if CONFIG_GCORE_RESUME_EVENT_NOTIFY
	queue_delayed_work(fn_data.gdev->gtp_workqueue,
		&fn_data.gdev->resume_notify_work, msecs_to_jiffies(200));
#endif

	gdev_kit->fw_update_state = false;
	g_ret_update_boot = 1;
	retry_count_boot = 0;

	return;

retry:
	retry_count_boot++;
	gdev_kit->fw_update_state = false;
	if (retry_count_boot < MAX_FW_RETRY_NUM) {
		GTP_ERROR("hostdl retry times:%d", retry_count_boot);
		queue_delayed_work(fn_data.gdev->fwu_workqueue,
			&fn_data.gdev->fwu_boot_work, msecs_to_jiffies(100));
	} else {
		gcore_trigger_esd_recovery();
		retry_count_boot = 0;
	}
}
#endif

static void gcore_get_fw_name(char *file_name)
{
	char *firmware_form = ".bin";

	strncat(file_name, gdev_kit->tskit_data->project_id,
		strlen(gdev_kit->tskit_data->project_id) + 1);
	strncat(gdev_kit->factory_fw_name, file_name, strlen(file_name) + 1);
	strncat(file_name, ".bin", strlen(".bin"));
	snprintf(gdev_kit->firmware_name, sizeof(gdev_kit->firmware_name), "ts/%s",
		file_name);
	GTP_DEBUG("%s, firmware name: %s", __func__, file_name);
}


int gcore_fw_update_boot(char *file_name)
{
	GTP_DEBUG("fw_update_boot: enter");
	gdev_kit->touch_irq = gdev_kit->platformkit_data->irq_id;
	GTP_DEBUG("touch_irq = %d,irq_id=%d", gdev_kit->touch_irq,
		gdev_kit->platformkit_data->irq_id);
	gcore_get_fw_name(file_name);
#ifdef CONFIG_BOOT_UPDATE_FIRMWARE_BY_ARRAY
	queue_delayed_work(gdev_kit->fwu_workqueue, &gdev_kit->fwu_boot_work,
		msecs_to_jiffies(STARTUP_UPDATE_DELAY_BOOT_MS));
#else
	queue_delayed_work(gdev_kit->fwu_workqueue, &gdev_kit->fwu_work,
		msecs_to_jiffies(STARTUP_UPDATE_DELAY_BOOT_MS));
#endif
	GTP_DEBUG("fw_update_boot: exit");
	return 0;
}

int dump_mp_data_to_kit_seq_file(struct seq_file *m, const u16 *data, int rows,
	int cols, enum MP_DATA_TYPE types)
{
	struct file *file;
	int r = 0;
	int ret = 0;
	const u16 *n_val = data;
	char linebuf[256];
	char y_prefix[10];
	char topic[30];
	int len;

	switch (types) {
	case RAW_DATA:
		GTP_DEBUG("dump Raw data to csv file");
		strncpy(topic, "Rawdata:, ", sizeof(topic));
		break;

	case RAWDATA_DECH:
		strncpy(topic, "rawdata_per_h:, ", sizeof(topic));
		break;

	case RAWDATA_DECL:
		strncpy(topic, "rawdata_per_l:, ", sizeof(topic));
		break;

	case NOISE_DATA:
		GTP_DEBUG("dump Noise data to csv file");
		strncpy(topic, "Noise:, ", sizeof(topic));
		break;

	case OPEN_DATA:
		GTP_DEBUG("dump Open data to csv file");
		strncpy(topic, "Open:, ", sizeof(topic));
		break;

	case SHORT_DATA:
		GTP_DEBUG("dump Short data to csv file");
		strncpy(topic, "Short:, ", sizeof(topic));
		break;

	default:
		strncpy(topic, "Rawdata:, ", sizeof(topic));
		break;
	}

	len = dump_excel_head_row_to_buffer(linebuf, sizeof(linebuf), cols, topic,
		"\n", ',');
	seq_puts(m, linebuf);
	memset(linebuf, 0, sizeof(linebuf));
	for (r = 0; r < rows; r++) {
		scnprintf(y_prefix, sizeof(y_prefix), "Y%d, ", r + 1);
		len = dump_mp_data_row_to_buffer(linebuf, sizeof(linebuf), n_val, cols,
			y_prefix, "\n", ',');
		firstline = 0;
		seq_puts(m, linebuf);
		memset(linebuf, 0, sizeof(linebuf));
		n_val += cols;
	}
	firstline = 1;

	switch (types) {
	case RAW_DATA:
		len = scnprintf(linebuf, sizeof(linebuf),
			"Max:, %4u, ,Min:, %4u, ,Threshold:, %4u, %4u, \n\n",
			g_mp_data->rawdata_node_max, g_mp_data->rawdata_node_min,
			g_mp_data->rawdata_range_min, g_mp_data->rawdata_range_max);
		break;

	case RAWDATA_DECH:
		len = scnprintf(linebuf, sizeof(linebuf),
			"Max:, %4u, ,Min:, %4u, ,Threshold:, %4u%%, \n\n",
			g_mp_data->rawdata_dech_max, g_mp_data->rawdata_dech_min,
			g_mp_data->rawdata_per);
		break;

	case RAWDATA_DECL:
		len = scnprintf(linebuf, sizeof(linebuf),
			"Max:, %4u, ,Min:, %4u, ,Threshold:, %4u%%, \n\n",
			g_mp_data->rawdata_decl_max, g_mp_data->rawdata_decl_min,
			g_mp_data->rawdata_per);
		break;

	case OPEN_DATA:
		len = scnprintf(linebuf, sizeof(linebuf), "Threshold:, %4u, \n\n",
			g_mp_data->open_min);
		break;

	case SHORT_DATA:
		len = scnprintf(linebuf, sizeof(linebuf), "Threshold:, %4u, \n\n",
			g_mp_data->short_min);
		break;

	case NOISE_DATA:
		len = scnprintf(linebuf, sizeof(linebuf), "Threshold:, %4u, \n\n",
			g_mp_data->peak_to_peak);
		break;

	default:
		len = 0;
		break;
	}

	seq_puts(m, linebuf);
	memset(linebuf, 0, sizeof(linebuf));

	return ret;
}


static int gcore_get_rawdata(struct ts_rawdata_info *info,
	struct ts_cmd_node *out_cmd)
{
	int retval;
	if ((info == NULL) || (out_cmd == NULL))
		return -EINVAL;
	GTP_DEBUG("get_rawdata :Start");
	retval = gcore_kit_start_mp_test(info);
	GTP_DEBUG("get_rawdata: End");
	gdev_kit->platformkit_data->chip_data->is_direct_proc_cmd = 0;
	GTP_DEBUG("Set is_direct_proc_cmd = :%d",
		gdev_kit->platformkit_data->chip_data->is_direct_proc_cmd);
	return retval;
}

static int gcore_get_capacitance_test_type(struct ts_test_type_info *info)
{
	struct ts_kit_device_data *chip_data = NULL;
	chip_data = gdev_kit->tskit_data->ts_platform_data->chip_data;

	GTP_DEBUG("get_capacitance_test_type: enter");
	if (!info) {
		GTP_ERROR("get_capacitance_test_type: info is null");
		return -EINVAL;
	}
	memcpy(info->tp_test_type, chip_data->tp_test_type, TS_CAP_TEST_TYPE_LEN);
	GTP_DEBUG("get_capacitance_test_type: test_type=%s", info->tp_test_type);
	gdev_kit->platformkit_data->chip_data->is_direct_proc_cmd = 1;
	GTP_DEBUG("Start Set is_direct_proc_cmd = :%d",
		gdev_kit->platformkit_data->chip_data->is_direct_proc_cmd);
	return NO_ERR;
}

static int gcore_rawdata_proc_printf(struct seq_file *m,
	struct ts_rawdata_info *info, int range_size, int row_size)
{
	int rows = RAWDATA_ROW;
	int cols = RAWDATA_COLUMN;
	int ret = 0;
	seq_puts(m, "MP Test begin\n");
	seq_puts(m, g_mp_data->info_buf);
	seq_puts(m, "\n");
	struct gcore_mp_data *mp_data = g_mp_data;
	if (mp_data->test_open) {
		ret = dump_mp_data_to_kit_seq_file(m, mp_data->open_node_val, rows,
			cols, OPEN_DATA);
		if (ret < 0) {
			GTP_ERROR("dump mp open test data to file failed");
			goto fail;
		}
	}

	if (mp_data->test_short) {
		ret = dump_mp_data_to_kit_seq_file(m, mp_data->short_node_val, rows,
			cols, SHORT_DATA);
		if (ret < 0) {
			GTP_ERROR("dump mp short test data to file failed");
			goto fail;
		}
	}

	if (mp_data->test_rawdata) {
		ret = dump_mp_data_to_kit_seq_file(m, mp_data->rawdata_node_val, rows,
			cols, RAW_DATA);
		if (ret < 0) {
			GTP_ERROR("dump mp test rawdata to file failed");
			goto fail;
		}
	}

	if (mp_data->test_noise) {
		ret = dump_mp_data_to_kit_seq_file(m, mp_data->noise_node_val, rows,
			cols, NOISE_DATA);
		if (ret < 0) {
			GTP_ERROR("dump mp noise test data to file failed");
			goto fail;
		}
	}

fail:
	return NO_ERR;
}
static int gcore_chip_get_info(struct ts_chip_info_param *info)
{
	int retval = NO_ERR;
	GTP_DEBUG("get chip info enter");
	if (info == NULL)
		return RESULT_ERR;

	snprintf(info->ic_vendor, GTP_CHIP_NAME_LEN, "galaxycore");
	snprintf(info->mod_vendor, GTP_CHIP_NAME_LEN, "%s",
		gdev_kit->tskit_data->project_id);
	snprintf(info->fw_vendor, GTP_CHIP_INFO_LENGTH * 2, "%x.%x.%x.%x",
		gdev_kit->fw_ver_in_bin[1], gdev_kit->fw_ver_in_bin[0],
		gdev_kit->fw_ver_in_bin[3], gdev_kit->fw_ver_in_bin[2]);

	return retval;
}
static int gcore_roi_switch(struct ts_roi_info *info)
{
	int ret = 0;
	int i = 0;
#ifdef ROI
	GTP_DEBUG("Enter roi switch");
	if (!info) {
		GTP_ERROR("info is null");
		ret = -ENOMEM;
		return ret;
	}

	switch (info->op_action) {
	case TS_ACTION_READ:
		info->roi_switch = gdev_kit->gtp_roi_switch;
		GTP_DEBUG("roi switch=%d\n", info->roi_switch);

		break;
	case TS_ACTION_WRITE:
		if (info->roi_switch)
			gdev_kit->gtp_roi_switch = 1;
		else
			gdev_kit->gtp_roi_switch = 0;
		GTP_DEBUG("roi switch=%d,gtp roi switch=%d", info->roi_switch, gdev_kit->gtp_roi_switch);
		if (!info->roi_switch) {
			for (i = 0; i < ROI_DATA_READ_LENGTH; i++)
				gcore_roi_data[i] = 0;
		}

		break;
	default:
		GTP_ERROR("invalid op action:%d", info->op_action);
		return -EINVAL;
	}
#endif
	return 0;
}

static unsigned char *gcore_roi_rawdata(void)
{
#ifdef ROI
	return (unsigned char *)gcore_roi_data;
#else
	return NULL;
#endif
}

struct ts_device_ops gcore_ts_device_ops = {
	.chip_detect = gcore_chip_detect,
	.chip_init = gcore_kit_chip_init,
	// .chip_parse_config = gcore_kit_parse_config,
	.chip_parse_config = NULL,
	.chip_input_config = gcore_kit_input_config,
	.chip_get_info = gcore_chip_get_info,
	.chip_irq_top_half = gcore_chip_irq_top_half,
	.chip_irq_bottom_half = gcore_chip_irq_bottom_half,
	.chip_reset = gcore_chip_reset,
	// .chip_shutdown = gcore_chip_shutdown,/
	.chip_fw_update_boot = gcore_fw_update_boot,
	// .chip_fw_update_sd = gcore_fw_update_sd,
	.chip_get_rawdata = gcore_get_rawdata,
	.chip_get_capacitance_test_type = gcore_get_capacitance_test_type,
	// .chip_get_calibration_data   = gcore_get_calibration_data,
	.chip_special_rawdata_proc_printf = gcore_rawdata_proc_printf,
	// .chip_wakeup_gesture_enable_switch = gcore_wakeup_gesture_enable_switch,
	.chip_suspend = gcore_kit_suspend,
	.chip_resume = gcore_kit_resume,
	.chip_charger_switch = gcore_charger_switch,
	.chip_horizon_switch = gcore_horizon_notify,
	.chip_roi_switch = gcore_roi_switch,
	.chip_roi_rawdata = gcore_roi_rawdata,
	// .chip_test = gcore_chip_test,
	// .chip_hw_reset = gcore_hw_reset,
	// .chip_wrong_touch = gcore_wrong_touch,
};


static int __init gcore_ts_module_init(void)
{
	struct device_node *child = NULL;
	struct device_node *root = NULL;
	int ret = NO_ERR;
	bool found = false;
	char project_id[64] = {0};

	GTP_DEBUG("touch module init");

	gdev_kit = kzalloc(sizeof(struct gcore_dev), GFP_KERNEL);
	if (gdev_kit == NULL) {
		GTP_ERROR("Alloc ts_data failed\n");
		return -ENOMEM;
	}

	gdev_kit->tskit_data =
		kzalloc(sizeof(struct ts_kit_device_data), GFP_KERNEL);
	if (gdev_kit->tskit_data == NULL) {
		GTP_ERROR("Alloc ts_kit_device_data failed\n");
		ret = -ENOMEM;
		goto out;
	}

	root = of_find_compatible_node(NULL, NULL, "huawei,ts_kit");
	if (root == NULL) {
		GTP_ERROR("Huawei ts_kit of node NOT found in DTS");
		ret = -ENODEV;
		goto out;
	}

	for_each_child_of_node(root, child)
	{
		if (of_device_is_compatible(child, "galaxycore")) {
			found = true;
			break;
		}
	}

	if (!found) {
		GTP_ERROR("device tree node not found");
		ret = -ENODEV;
		goto out;
	}

	gdev_kit->tskit_data->cnode = child;
	gdev_kit->tskit_data->ops = &gcore_ts_device_ops;
	ret = huawei_ts_chip_register(gdev_kit->tskit_data);
	if (ret) {
		GTP_ERROR("Register chip to ts_kit failed %d !", ret);
		goto out;
	}

	return 0;
out:
	if (gdev_kit) {
		if (gdev_kit->tskit_data) {
			kfree(gdev_kit->tskit_data);
			gdev_kit->tskit_data = NULL;
		}
		kfree(gdev_kit);
		gdev_kit = NULL;
	}
	GTP_ERROR("touch module init returns %d", ret);

	return ret;
}

static void __exit gcore_ts_module_exit(void)
{
	gcore_deinit(gdev_kit);
	if (gdev_kit->tskit_data) {
		kfree(gdev_kit->tskit_data);
		gdev_kit->tskit_data = NULL;
	}

	kfree(gdev_kit);
	gdev_kit = NULL;
}


late_initcall(gcore_ts_module_init);
module_exit(gcore_ts_module_exit);
MODULE_AUTHOR("Huawei Device Company");
MODULE_DESCRIPTION("Huawei TouchScreen Driver");
MODULE_LICENSE("GPL");
