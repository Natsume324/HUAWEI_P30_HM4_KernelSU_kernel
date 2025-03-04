/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2013-2020. All rights reserved.
 * Description: Sensor Hub Channel Bridge
 * Author: huangjisong
 * Create: 2013-3-10
 */

#include "contexthub_boot.h"

#include <linux/delay.h>
#include <linux/err.h>
#include <linux/hisi/hisi_rproc.h>
#include <linux/hisi/hisi_syscounter.h>
#include <linux/hisi/usb/hisi_usb.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/notifier.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/regulator/consumer.h>
#include <linux/workqueue.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#ifdef CONFIG_CONTEXTHUB_IDLE_32K
#include <linux/hisi/hisi_idle_sleep.h>
#endif

#ifdef CONFIG_HUAWEI_DSM
#include <dsm/dsm_pub.h>
#endif

#include <soc_pmctrl_interface.h>

#include "contexthub_pm.h"
#include "contexthub_recovery.h"
#include "contexthub_route.h"
#include "motion_detect.h"
#include "sensor_config.h"
#include "sensor_detect.h"
#include "tp_color.h"

/*
 * CONFIG_USE_CAMERA3_ARCH : the camera module build config
 * du to the sensor_1V8 by camera power chip
 */
#ifdef CONFIG_USE_CAMERA3_ARCH
#include <media/huawei/hw_extern_pmic.h>
#define SENSOR_1V8_POWER
#endif

int (*api_inputhub_mcu_recv)(const char *buf, unsigned int length) = 0;

extern uint32_t need_reset_io_power;
extern uint32_t need_set_3v_io_power;
extern atomic_t iom3_rec_state;

static int is_sensor_mcumode; /* mcu power mode: 0 power off;  1 power on */
static struct notifier_block notifier_blocker;
static int g_boot_iom3 = STARTUP_IOM3_CMD;

struct completion iom3_reboot;
struct config_on_ddr *g_p_config_on_ddr;
struct regulator *sensorhub_vddio;
int sensor_power_pmic_flag;
int sensor_power_init_finish;
static u8 tplcd_manufacture;       // get from dts
static u8 tplcd_manufacture_curr; // get from /sys/class/graphics/fb0/lcd_model
unsigned long sensor_jiffies;
rproc_id_t ipc_ap_to_iom_mbx = HISI_AO_ACPU_IOM3_MBX1;
rproc_id_t ipc_ap_to_lpm_mbx = HISI_ACPU_LPM3_MBX_5;
rproc_id_t ipc_iom_to_ap_mbx = HISI_AO_IOM3_ACPU_MBX1;
int sensorhub_reboot_reason_flag = SENSOR_POWER_DO_RESET;

#ifdef CONFIG_CONTEXTHUB_IDLE_32K
#define PERI_USED_TIMEOUT (jiffies + HZ / 100)
#define LOWER_LIMIT 0
#define UPPER_LIMIT 255
#define borderline_upper_protect(a, b) (((a) == (b)) ? (a) : ((a) + 1))
#define borderline_lower_protect(a, b) (((a) == (b)) ? (a) : ((a) - 1))
struct timer_list peri_timer;
static unsigned int peri_used_t;
static unsigned int peri_used;
spinlock_t peri_lock;

static void peri_used_timeout(unsigned long data)
{
	int ret;
	unsigned long flags;

	spin_lock_irqsave(&peri_lock, flags);
	pr_debug("%s used %d, t %d\n", __func__, peri_used, peri_used_t);
	if (peri_used == 0) {
		ret = hisi_idle_sleep_vote(ID_IOMCU, 0);
		if (ret)
			pr_err("%s hisi_idle_sleep_vote err\n", __func__);
		peri_used_t = 0;
	}
	spin_unlock_irqrestore(&peri_lock, flags);
}
#endif

u8 get_tplcd_manufacture(void)
{
	return tplcd_manufacture;
}

u8 get_tplcd_manufacture_curr(void)
{
	return tplcd_manufacture_curr;
}

static void peri_used_init(void)
{
#ifdef CONFIG_CONTEXTHUB_IDLE_32K
	spin_lock_init(&peri_lock);
	setup_timer(&peri_timer, peri_used_timeout, 0);
#endif
}

void peri_used_request(void)
{
#ifdef CONFIG_CONTEXTHUB_IDLE_32K
	int ret;
	unsigned long flags;

	del_timer_sync(&peri_timer);
	spin_lock_irqsave(&peri_lock, flags);
	pr_debug("%s used %d,t %d\n", __func__, peri_used, peri_used_t);
	if (peri_used != 0) {
		peri_used = borderline_upper_protect(peri_used, UPPER_LIMIT);
		spin_unlock_irqrestore(&peri_lock, flags);
		return;
	}

	if (peri_used_t == 0) {
		ret = hisi_idle_sleep_vote(ID_IOMCU, 1);
		if (ret)
			pr_err("%s hisi_idle_sleep_vote err\n", __func__);
	}

	peri_used = 1;
	peri_used_t = 1;
	spin_unlock_irqrestore(&peri_lock, flags);
#endif
}

void peri_used_release(void)
{
#ifdef CONFIG_CONTEXTHUB_IDLE_32K
	unsigned long flags;

	spin_lock_irqsave(&peri_lock, flags);
	pr_debug("%s used %d\n", __func__, peri_used);
	peri_used = borderline_lower_protect(peri_used, LOWER_LIMIT);
	mod_timer(&peri_timer, PERI_USED_TIMEOUT);
	spin_unlock_irqrestore(&peri_lock, flags);
#endif
}

#ifdef CONFIG_HUAWEI_DSM
struct dsm_client *shb_dclient;

struct dsm_client *inputhub_get_shb_dclient(void)
{
	return shb_dclient;
}
#endif

#ifdef SENSOR_1V8_POWER
extern int hw_extern_pmic_query_state(int index, int *state);
#else
static int hw_extern_pmic_query_state(int index, int *state)
{
	hwlog_err("the camera power cfg donot define\n");
	return 1;
}
#endif

int get_sensor_mcu_mode(void)
{
	return is_sensor_mcumode;
}
EXPORT_SYMBOL(get_sensor_mcu_mode);

static void set_sensor_mcumode(int mode)
{
	is_sensor_mcumode = mode;
}

static bool recovery_mode_skip_load(void)
{
	int len = 0;
	struct device_node *recovery_node = NULL;
	const char *recovery_attr = NULL;

	if (!strstr(saved_command_line, "recovery_update=1"))
		return false;

	recovery_node = of_find_compatible_node(NULL, NULL, "hisilicon,recovery_iomcu_image_skip");
	if (!recovery_node)
		return false;

	recovery_attr = of_get_property(recovery_node, "status", &len);
	if (!recovery_attr)
		return false;

	if (strcmp(recovery_attr, "ok") != 0)
		return false;

	return true;
}

int is_sensorhub_disabled(void)
{
	int len = 0;
	struct device_node *sh_node = NULL;
	const char *sh_status = NULL;
	static int ret;
	static int once;

	if (once)
		return ret;

	if (recovery_mode_skip_load()) {
		hwlog_err("%s: recovery update mode, do not start sensorhub\n", __func__);
		once = 1;
		ret = -1;
		return ret;
	}

	sh_node = of_find_compatible_node(NULL, NULL,
		"huawei,sensorhub_status");
	if (!sh_node) {
		hwlog_err("%s, can not find sensorhub_status n", __func__);
		return -1;
	}

	sh_status = of_get_property(sh_node, "status", &len);
	if (!sh_status) {
		hwlog_err("%s, can't find property status\n", __func__);
		return -1;
	}

	if (strstr(sh_status, "ok")) {
		hwlog_info("%s, sensorhub enabled\n", __func__);
		ret = 0;
	} else {
		hwlog_info("%s, sensorhub disabled\n", __func__);
		ret = -1;
	}
	once = 1;
	return ret;
}

#ifdef SENSOR_1V8_POWER
static int check_sensor_1v8_from_pmic(void)
{
	int i;
	int ret;
	int len = 0;
	int state = 0;
	int sensor_1v8_ldo = 0;
	int sensor_1v8_flag = 0;
	const char *sensor_1v8_from_pmic = NULL;
	struct device_node *sensorhub_node = NULL;

	sensorhub_node = of_find_compatible_node(NULL, NULL, "huawei,sensorhub");
	if (!sensorhub_node) {
		hwlog_err("%s, can't find node sensorhub\n", __func__);
		return 0;
	}
	sensor_1v8_from_pmic = of_get_property(sensorhub_node, "sensor_1v8_from_pmic", &len);
	if (!sensor_1v8_from_pmic) {
		hwlog_info("%s, find sensor_1v8_from_pmic err\n", __func__);
		return 1;
	}
	sensor_power_pmic_flag = 1;
	if (strstr(sensor_1v8_from_pmic, "yes")) {
		ret = of_property_read_u32(sensorhub_node, "sensor_1v8_ldo", &sensor_1v8_ldo);
		if (ret == 0) {
			hwlog_info("%s,read sensor_1v8_ldo %d succ\n", __func__, sensor_1v8_ldo);
			sensor_1v8_flag = 1;
		} else {
			hwlog_err("%s,read sensor_1v8_ldo fail\n", __func__);
			return 0;
		}
	} else {
		hwlog_info("%s, sensor_1v8 not from pmic\n", __func__);
		return 1;
	}

	if (sensor_1v8_flag) {
		for (i = 0; i < 10; i++) {
			ret = hw_extern_pmic_query_state(sensor_1v8_ldo, &state);
			if (state) {
				hwlog_info("sensor_1V8 set high succ\n");
				break;
			}
			msleep(200);
		}
		if (i == 10 && state == 0) {
			hwlog_err("sensor_1V8 set high fail,ret:%d\n", ret);
			return 0;
		}
	}
	sensor_power_init_finish = 1;
	return 1;
}
#endif

int sensor_pmic_power_check(void)
{
	int ret;
	int state = 0;
	int result = SENSOR_POWER_STATE_OK;

	if (!sensor_power_init_finish || is_sensorhub_disabled()) {
		result = SENSOR_POWER_STATE_INIT_NOT_READY;
		goto out;
	}
	if (!sensor_power_pmic_flag) {
		result =  SENSOR_POWER_STATE_NOT_PMIC;
		goto out;
	}

	ret = hw_extern_pmic_query_state(1, &state);
	if (ret) {
		result = SENSOR_POWER_STATE_CHECK_ACTION_FAILED;
		goto out;
	}
	if (!state)
		result = SENSOR_POWER_STATE_CHECK_RESULT_FAILED;
out:
	hwlog_info("sensor check result:%d\n", result);
	return result;
}

static lcd_module lcd_info[] = {
	{ DTS_COMP_JDI_NT35695_CUT3_1, JDI_TPLCD },
	{ DTS_COMP_LG_ER69006A, LG_TPLCD },
	{ DTS_COMP_JDI_NT35695_CUT2_5, JDI_TPLCD },
	{ DTS_COMP_LG_ER69007, KNIGHT_BIEL_TPLCD },
	{ DTS_COMP_SHARP_NT35597, KNIGHT_BIEL_TPLCD },
	{ DTS_COMP_SHARP_NT35695, KNIGHT_BIEL_TPLCD },
	{ DTS_COMP_LG_ER69006_FHD, KNIGHT_BIEL_TPLCD },
	{ DTS_COMP_MIPI_BOE_ER69006, KNIGHT_LENS_TPLCD },
	{ DTS_COMP_BOE_OTM1906C, BOE_TPLCD },
	{ DTS_COMP_INX_OTM1906C, INX_TPLCD },
	{ DTS_COMP_EBBG_OTM1906C, EBBG_TPLCD },
	{ DTS_COMP_JDI_NT35695, JDI_TPLCD },
	{ DTS_COMP_LG_R69006, LG_TPLCD },
	{ DTS_COMP_SAMSUNG_S6E3HA3X02, SAMSUNG_TPLCD },
	{ DTS_COMP_LG_R69006_5P2, LG_TPLCD },
	{ DTS_COMP_SHARP_NT35695_5P2, SHARP_TPLCD },
	{ DTS_COMP_JDI_R63452_5P2, JDI_TPLCD },
	{ DTS_COMP_SAM_WQ_5P5, BIEL_TPLCD },
	{ DTS_COMP_SAM_FHD_5P5, VITAL_TPLCD },
	{ DTS_COMP_JDI_R63450_5P7, JDI_TPLCD },
	{ DTS_COMP_SHARP_DUKE_NT35597, SHARP_TPLCD },
	{ DTS_COMP_BOE_NT36682A_6P59, BOE_TPLCD },
	{ DTS_COMP_INX_NT36682A, INX_TPLCD },
	{ DTS_COMP_TCL_NT36682A, TCL_TPLCD },
	{ DTS_COMP_TM_NT36682A, TM_TPLCD },
	{ DTS_COMP_BOE_TD4320_6P59, BOE_TPLCD },
	{ DTS_COMP_TM_TD4321_6P59, TM_TPLCD },
	{ DTS_COMP_TM_FT8615_6P39, TM_TPLCD },
	{ DTS_COMP_BOE_FT8009_6P39, LG_TPLCD },
	{ DTS_COMP_BOE_FT8615_6P39, BOE_TPLCD },
	{ DTS_COMP_INX_NT36572A_6P39, INX_TPLCD },
	{ DTS_COMP_INX_NT36526_6P39, INX_TPLCD2 },
	{ DTS_COMP_TM_FT8615_G6_6P39, TM_TPLCD },
	{ DTS_COMP_SAMSUNG_AMS653VY01, SAMSUNG_TPLCD },
	{ DTS_COMP_EDO_E652FAC73, EDO_TPLCD },
};

static lcd_model lcd_model_info[] = {
	{ DTS_COMP_AUO_OTM1901A_5P2, AUO_TPLCD },
	{ DTS_COMP_AUO_TD4310_5P2, AUO_TPLCD },
	{ DTS_COMP_TM_FT8716_5P2, TM_TPLCD },
	{ DTS_COMP_EBBG_NT35596S_5P2, EBBG_TPLCD },
	{ DTS_COMP_JDI_ILI7807E_5P2, JDI_TPLCD },
	{ DTS_COMP_CMI_NT36682A, CMI_TPLCD },
	{ DTS_COMP_JDI_TD4320, JDI_TPLCD },
	{ DTS_COMP_BOE_TD4320, BOE_TPLCD },
	{ DTS_COMP_BOE_NT36682A, BOE_TPLCD },
	{ DTS_COMP_TM_TD4320, TM_TPLCD },
	{ DTS_COMP_TM_TD4330, TM_TPLCD },
	{ DTS_COMP_TM_NT36682A_6P15, TM_TPLCD },
	{ DTS_COMP_AUO_TD4320_6P4, AUO_TPLCD },
	{ DTS_COMP_CTC_NT36682A_6P4, CTC_TPLCD },
	{ DTS_COMP_TM_NT36682A_6P4, TM_TPLCD },
	{ DTS_COMP_BOE_TD4320_6P4, BOE_TPLCD },
};

const static lcd_panel_model g_lcd_panel_model_list[] = {
	{ "320_506 6.7' VIDEO 720 x 1600", TS_PANEL_LENS },
	{ "250_105 6.7' VIDEO 720 x 1600", TS_PANEL_TXD },
	{ "190_107 6.7' VIDEO 720 x 1600", TS_PANEL_BOE },
	{ "190_A05 6.7' VIDEO 720 x 1600", TS_PANEL_BOE },
	{ "220_107 6.7' VIDEO 720 x 1600", TS_PANEL_KING },
	{ "110_107 6.7' VIDEO 720 x 1600", TS_PANEL_TIANMA },
	{ "BACH4_120 20d 0 10.3' 1200 x 2000", TS_PANEL_EELY },
	{ "BACH4_190 502 0 10.3' 1200 x 2000", TS_PANEL_TRULY },
	{ "BACH4_190 20d 0 10.3' 1200 x 2000", TS_PANEL_TRULY },
	{ "370_606 6.5' VIDEO 720 x 1600", TS_PANEL_SKYWORTH },
	{ "110_d05 6.5' VIDEO 720 x 1600", TS_PANEL_TIANMA },
	{ "250_107_D0 6.7' VIDEO 720 x 1600", TS_PANEL_BOE0 },
	{ "250_107_D1 6.7' VIDEO 720 x 1600", TS_PANEL_BOE1 },
	{ "380_a0a 6.7' VIDEO 720 x 1600", TS_PANEL_HUAJIACAI },
	{ "320_g00 6.7' VIDEO 720 x 1600", TS_PANEL_GALAXYCORE },
	{ "320_506_1 6.7' VIDEO 720 x 1600", TS_PANEL_LENS1 },
};

uint16_t get_lcd_panel_model_list_size(void)
{
	return (uint16_t)ARRAY_SIZE(g_lcd_panel_model_list);
}

static int8_t get_lcd_info(uint8_t index)
{
	int ret;
	struct device_node *np = NULL;

	np = of_find_compatible_node(NULL, NULL, lcd_info[index].dts_comp_mipi);
	ret = of_device_is_available(np);
	if (np && ret)
		return lcd_info[index].tplcd;
	else
		return -1;
}

static int8_t get_lcd_model(const char *lcd_model, uint8_t index)
{
	if (!strncmp(lcd_model, lcd_model_info[index].dts_comp_lcd_model,
		strlen(lcd_model_info[index].dts_comp_lcd_model)))
		return lcd_model_info[index].tplcd;
	else
		return -1;
}

static int get_lcd_module(void)
{
	uint8_t index;
	int8_t tplcd;
	struct device_node *np = NULL;
	const char *lcd_model = NULL;

	for (index = 0; index < ARRAY_SIZE(lcd_info); index++) {
		tplcd = get_lcd_info(index);
		if (tplcd > 0)
			return tplcd;
	}

	np = of_find_compatible_node(NULL, NULL, "huawei,lcd_panel_type");
	if (!np) {
		hwlog_err("not find lcd_panel_type node\n");
		return -1;
	}
	if (of_property_read_string(np, "lcd_panel_type", &lcd_model)) {
		hwlog_err("not find lcd_model in dts\n");
		return -1;
	}
	hwlog_info("find lcd_panel_type suc in dts\n");

	for (index = 0; index < ARRAY_SIZE(lcd_model_info); index++) {
		tplcd = get_lcd_model(lcd_model, index);
		if (tplcd > 0)
			return tplcd;
	}

	hwlog_warn("sensor kernel failed to get lcd module\n");
	return -1;
}

static uint16_t find_lcd_manufacture(const char *panel_name)
{
	const int len = get_lcd_panel_model_list_size();
	int i = 0;
	for (i = 0; i < len; i++) {
		if ((strncmp(g_lcd_panel_model_list[i].panel_name, panel_name, 
				strlen(g_lcd_panel_model_list[i].panel_name)) == 0)) {
			hwlog_err("match panel_name success, index = %d, manufacture = %d", i,
				g_lcd_panel_model_list[i].manufacture);
			return (uint16_t)g_lcd_panel_model_list[i].manufacture;
		}
	}
	return TS_PANEL_UNKNOWN;
}

void init_tp_manufacture_curr(void)
{
	int ret = -1;
	char buf[LCD_PANEL_NAME_LEN + 1] = {'\0'};
	char path[128] = "/sys/class/graphics/fb0/lcd_model";
	struct file *fp = NULL;
	mm_segment_t old_fs;
	u8 lcd_manuf = get_lcd_module();

	tplcd_manufacture_curr = lcd_manuf;

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	fp = filp_open(path, O_RDONLY, 0);
	if (IS_ERR(fp)) {
		hwlog_err("FAIL TO OPEN %s FILE", path);
		set_fs(old_fs);
		return;
	}

	vfs_llseek(fp, 0L, SEEK_SET);
	ret = vfs_read(fp, buf, LCD_PANEL_NAME_LEN, &(fp->f_pos));
	if (ret < 0) {
		hwlog_err("%s:read file %s exception with ret %d.\n",
			__func__, path, ret);
		set_fs(old_fs);
		return;
	}
	filp_close(fp, NULL);

	set_fs(old_fs);

	lcd_manuf = find_lcd_manufacture(buf);
	if (lcd_manuf == TS_PANEL_UNKNOWN) {
		return;
	}

	tplcd_manufacture_curr = lcd_manuf;
	return;
}


static int inputhub_mcu_recv(const char *buf, unsigned int length)
{
	if (atomic_read(&iom3_rec_state) == IOM3_RECOVERY_START) {
		hwlog_err("iom3 under recovery mode, ignore all recv data\n");
		return 0;
	}

	if (!api_inputhub_mcu_recv) {
		hwlog_err("---->error: api_inputhub_mcu_recv == NULL\n");
		return -1;
	}

	return api_inputhub_mcu_recv(buf, length);
}

/* received data from mcu. */
static int mbox_recv_notifier(struct notifier_block *notifier_blocker,
	unsigned long len, void *msg)
{
	inputhub_mcu_recv(msg, len * sizeof(int)); /* convert to bytes */
	return 0;
}

static int inputhub_mcu_connect(void)
{
	int ret;
	/* connect to inputhub_route */
	api_inputhub_mcu_recv = inputhub_route_recv_mcu_data;

	notifier_blocker.next = NULL;
	notifier_blocker.notifier_call = mbox_recv_notifier;

	/* register the rx notify callback */
	ret = RPROC_MONITOR_REGISTER(ipc_iom_to_ap_mbx, &notifier_blocker);
	if (ret)
		hwlog_info("%s:RPROC_MONITOR_REGISTER failed", __func__);
	return 0;
}

static int sensorhub_img_dump(int type, void *buff, int size)
{
	return 0;
}

#ifdef CONFIG_HUAWEI_DSM
struct dsm_client_ops sensorhub_ops = {
	.poll_state = NULL,
	.dump_func = sensorhub_img_dump,
};

struct dsm_dev dsm_sensorhub = {
	.name = "dsm_sensorhub",
	.device_name = NULL,
	.ic_name = NULL,
	.module_name = NULL,
	.fops = &sensorhub_ops,
	.buff_size = 1024,
};

struct dsm_dev *get_dsm_sensorhub(void)
{
	return &dsm_sensorhub;
}
#endif

static int boot_iom3(void)
{
	int ret;

	peri_used_request();
	ret = RPROC_ASYNC_SEND(ipc_ap_to_iom_mbx, (mbox_msg_t *)&g_boot_iom3, 1);
	peri_used_release();
	if (ret)
		hwlog_err("RPROC_ASYNC_SEND error in %s\n", __func__);
	return ret;
}

void write_timestamp_base_to_sharemem(void)
{
	u64 syscnt;
	u64 kernel_ns;
	struct timespec64 ts;

	get_monotonic_boottime64(&ts);
#ifdef CONFIG_BSP_SYSCOUNTER
	syscnt = hisi_get_syscount();
#endif
	kernel_ns = (u64)(ts.tv_sec * NSEC_PER_SEC) + (u64)ts.tv_nsec;

	g_p_config_on_ddr->timestamp_base.syscnt = syscnt;
	g_p_config_on_ddr->timestamp_base.kernel_ns = kernel_ns;
}

static int mcu_sys_ready_callback(const struct pkt_header *head)
{
	int ret;
	unsigned int time_of_vddio_power_reset;
	unsigned long new_sensor_jiffies;
#ifdef SENSOR_1V8_POWER
	int result;
#endif
	if (((pkt_sys_statuschange_req_t *)head)->status == ST_MINSYSREADY) {
		tplcd_manufacture = get_lcd_module();
		init_tp_manufacture_curr();
		hwlog_info("sensor get_lcd_module tplcd_manufacture=%d, curr:%d\n", 
			tplcd_manufacture, tplcd_manufacture_curr);
#ifdef SENSOR_1V8_POWER
		result = check_sensor_1v8_from_pmic();
		if (!result)
			hwlog_err("check sensor_1V8 from pmic fail\n");
#endif
		hwlog_info("need_reset_io_power:%d reboot_reason_flag:%d\n", need_reset_io_power, sensorhub_reboot_reason_flag);
		if (need_reset_io_power && (sensorhub_reboot_reason_flag == SENSOR_POWER_DO_RESET)) {
			new_sensor_jiffies = jiffies - sensor_jiffies;
			time_of_vddio_power_reset = jiffies_to_msecs(new_sensor_jiffies);
			if (time_of_vddio_power_reset < SENSOR_MAX_RESET_TIME_MS)
				msleep(SENSOR_MAX_RESET_TIME_MS - time_of_vddio_power_reset);

			if (need_set_3v_io_power) {
				ret = regulator_set_voltage(sensorhub_vddio, SENSOR_VOLTAGE_3V, SENSOR_VOLTAGE_3V);
				if (ret < 0)
					hwlog_err("set sensorhub volt err\n");
			}
			hwlog_info("time_of_vddio_power_reset %u\n", time_of_vddio_power_reset);
			ret = regulator_enable(sensorhub_vddio);
			if (ret < 0)
				hwlog_err("sensor vddio enable 2.85V\n");

			msleep(SENSOR_DETECT_AFTER_POWERON_TIME_MS);
		}
		ret = init_sensors_cfg_data_from_dts();
		if (ret)
			hwlog_err("get cfg data fail,ret=%d, use def\n", ret);
	} else if (((pkt_sys_statuschange_req_t *)head)->status == ST_MCUREADY) {
		ret = sensor_set_cfg_data();
		if (ret < 0)
			hwlog_err("sensor_chip_detect ret=%d\n", ret);
		ret = sensor_set_fw_load();
		if (ret < 0)
			hwlog_err("sensor fw dload err ret=%d\n", ret);
		ret = motion_set_cfg_data();
		if (ret < 0)
			hwlog_err("motion set cfg data err ret=%d\n", ret);
		unregister_mcu_event_notifier(TAG_SYS, CMD_SYS_STATUSCHANGE_REQ, mcu_sys_ready_callback);
		atomic_set(&iom3_rec_state, IOM3_RECOVERY_IDLE);
	}
	return 0;
}

static void set_notifier(void)
{
	register_mcu_event_notifier(TAG_SYS, CMD_SYS_STATUSCHANGE_REQ,
		mcu_sys_ready_callback);
	register_mcu_event_notifier(TAG_SYS, CMD_SYS_STATUSCHANGE_REQ,
		iom3_rec_sys_callback);
	set_pm_notifier();
}

static void read_reboot_reason_cmdline(void)
{
	char reboot_reason_buf[SENSOR_REBOOT_REASON_MAX_LEN] = {0};
	char *pstr = NULL;
	char *dstr = NULL;

	pstr = strstr(saved_command_line, "reboot_reason=");
	if (!pstr) {
		pr_err("No fastboot reboot_reason info\n");
		return;
	}
	pstr += strlen("reboot_reason=");
	dstr = strstr(pstr, " ");
	if (!dstr) {
		pr_err("No find the reboot_reason end\n");
		return;
	}
	memcpy(reboot_reason_buf, pstr, (unsigned long)(dstr - pstr));
	reboot_reason_buf[dstr - pstr] = '\0';

	if (!strcasecmp(reboot_reason_buf, "AP_S_COLDBOOT"))
		sensorhub_reboot_reason_flag = SENSOR_POWER_NO_RESET;
	else
		sensorhub_reboot_reason_flag = SENSOR_POWER_DO_RESET;

	hwlog_info("sensorhub get reboot reason:%s length:%d flag:%d\n",
		reboot_reason_buf, (int)strlen(reboot_reason_buf),
		sensorhub_reboot_reason_flag);
}

static int write_defualt_config_info_to_sharemem(void)
{
	if (!g_p_config_on_ddr)
		g_p_config_on_ddr = (struct config_on_ddr *)ioremap_wc(IOMCU_CONFIG_START, IOMCU_CONFIG_SIZE);

	if (!g_p_config_on_ddr) {
		hwlog_err("ioremap %x failed in %s\n",
			IOMCU_CONFIG_START, __func__);
		return -1;
	}

	memset(g_p_config_on_ddr, 0, sizeof(struct config_on_ddr));
	g_p_config_on_ddr->log_buff_cb_backup.mutex = 0;
	g_p_config_on_ddr->log_level = INFO_LEVEL;
	return 0;
}

static int inputhub_mcu_init(void)
{
	int ret;

	peri_used_init();
	if (write_defualt_config_info_to_sharemem())
		return -1;
	write_timestamp_base_to_sharemem();
	read_tp_color_cmdline();
	read_reboot_reason_cmdline();
	sensorhub_io_driver_init();

	if (is_sensorhub_disabled())
		return -1;

#ifdef CONFIG_HUAWEI_DSM
	shb_dclient = dsm_register_client(&dsm_sensorhub);
#endif
	init_completion(&iom3_reboot);
	recovery_init();
	sensor_redetect_init();
	inputhub_route_init();
	set_notifier();
	inputhub_mcu_connect();
	ret = boot_iom3();
	if (ret)
		hwlog_err("%s boot sensorhub fail,ret %d.\n", __func__, ret);
	set_sensor_mcumode(1);
	mag_current_notify();
	return ret;
}

static void __exit inputhub_mcu_exit(void)
{
	inputhub_route_exit();
	RPROC_PUT(ipc_ap_to_iom_mbx);
	RPROC_PUT(ipc_iom_to_ap_mbx);
}

late_initcall(inputhub_mcu_init);
module_exit(inputhub_mcu_exit);

MODULE_AUTHOR("Input Hub <smartphone@huawei.com>");
MODULE_DESCRIPTION("input hub bridge");
MODULE_LICENSE("GPL");
