

#include <linux/delay.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/gpio.h>
#include <linux/jiffies.h>
#include <linux/module.h>
#include <linux/mtd/hisi_nve_interface.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/regulator/consumer.h>
#include <linux/rtc.h>
#include <linux/slab.h>
#include <linux/types.h>

#include <securec.h>
#ifdef CONFIG_HUAWEI_DSM
#include <dsm/dsm_pub.h>
#endif

#include "protocol.h"

#include "als_detect.h"
#include "cap_prox_detect.h"
#include "contexthub_pm.h"
#include "contexthub_recovery.h"
#include "contexthub_route.h"
#include "motion_channel.h"
#include "sensor_info.h"
#include "sensor_sysfs.h"

/* Wait 100ms for data transmitting */
#define FINGERSENSE_TRANS_TOUT             msecs_to_jiffies(100)
/* We think the data is fresh if it is collected in 10ms */
#define FINGERSENSE_FRESH_TIME             msecs_to_jiffies(10)
#define CALIBRATE_DATA_LENGTH              15
#define ACC_GYRO_OFFSET_CALIBRATE_LENGTH   3
#define ACC_PERIOD_MAX                     20
#define MAX_VALUE                          4294967296
#define MAX_AIRPRESS_OFFSET                500
#define MIN_AIRPRESS_OFFSET                (-500)
#define GYRO_RANGE_1000DPS                 1000
#define GYRO_DEFAULT_RANGE                 2000
#define GYRO_RANGE_FROM_2000DPS_TO_1000DPS 2
#define GYRO_PERIOD_MAX                    10
#define ALS_PERIOD_MAX                     100
#define HOLSTER_DOUBLE_HALL_MODE           2
#define DOUBLE_HALL_BIT_0                  0x01
#define DOUBLE_HALL_BIT_3                  0x08

#define MID_PS          1
#define NEAR_PS         2
#define PS_RATIO        1000
#define PS_MODE_MAX     2
#define PS_MODE_MIN     0

enum {
	ALS_CALI_ENV_LIGHT = 1,
	ALS_CALI_ENV_DARK = 2,
};

volatile int hall_value;
static int fingersense_enabled;
static int g_stop_auto_als;
static int g_stop_auto_ps;
int gyro_range = GYRO_DEFAULT_RANGE;
static uint8_t debug_read_data_buf[DEBUG_DATA_LENGTH] = { 0 };
static uint8_t i2c_rw16_data_buf[2] = { 0 };
static enum ret_type return_calibration = RET_INIT; /* acc calibrate result */
static int acc_close_after_calibrate = true;
static enum ret_type gyro_calibration_res = RET_INIT;      /* gyro calibrate result */
static enum ret_type ps_calibration_res = RET_INIT;        /* ps calibrate result */
static enum ret_type als_calibration_res = RET_INIT;       /* als calibrate result */
static enum ret_type handpress_calibration_res = RET_INIT; /* handpress calibrate result */
static enum ret_type return_cap_prox_calibration = RET_INIT;
static int als_close_after_calibrate = true;
static int gyro_close_after_calibrate = true;
static int32_t ps_calib_data[3] = { 0 };
static int32_t gyro_calib_data[15] = { 0 };
static int32_t set_gyro_calib_data[15] = { 0 };
static int32_t set_acc_calib_data[15] = { 0 };
static int cap_prox_calibrate_len;
static uint8_t cap_prox_calibrate_data[CAP_PROX_CALIDATA_NV_SIZE] = { 0 };
static struct work_struct cap_prox_calibrate_work;
static int airpress_cali_flag;
static struct work_struct handpress_calibrate_work;
static int32_t ps_calib_data_for_data_collect[4] = { 0 };
static int ps_switch_mode;
static uint8_t g_hp_offset[MAX_HP_OFFSET_DATA_LENGTH];

struct acc_gyr_offset_threshold acc_calib_threshold[CALIBRATE_DATA_LENGTH] = {
	{ -320, 320 },    /* x-offset unit:mg */
	{ -320, 320 },    /* y-offset unit:mg */
	{ -320, 320 },    /* z-offset unit:mg */
	{ 6500, 13500 },  /* x-sensitivity */
	{ 6500, 13500 },  /* y-sensitivity */
	{ 6500, 13500 },  /* z-sensitivity */
	/* The following is the interaxial interference */
	{ 6500,  13500 },
	{ -2000, 2000 },
	{ -2000, 2000 },
	{ -2000, 2000 },
	{ 6500,  13500 },
	{ -2000, 2000 },
	{ -2000, 2000 },
	{ -2000, 2000 },
	{ 6500,  13500 },
};
struct acc_gyr_offset_threshold gyro_calib_threshold[CALIBRATE_DATA_LENGTH] = {
	{ -572, 572 },    /* x-offset */
	{ -572, 572 },    /* y-offset */
	{ -572, 572 },    /* z-offset */
	{ 6500, 13500 },  /* x-sensitivity */
	{ 6500, 13500 },  /* y-sensitivity */
	{ 6500, 13500 },  /* z-sensitivity */
	/* The following is the interaxial interference */
	{ 6500,  13500 },
	{ -2000, 2000 },
	{ -2000, 2000 },
	{ -2000, 2000 },
	{ 6500,  13500 },
	{ -2000, 2000 },
	{ -2000, 2000 },
	{ -2000, 2000 },
	{ 6500,  13500 },
};

/* pass mark as NA */
static char *cali_error_code_str[] = {
	"NULL", "NA", "EXEC_FAIL", "NV_FAIL", "COMMU_FAIL", "RET_TYPE_MAX"
};
static unsigned long fingersense_data_ts; /* timestamp for the data */

extern struct hisi_nve_info_user user_info;
extern struct airpress_platform_data airpress_data;
extern union sar_calibrate_data sar_calibrate_datas;
extern struct sar_platform_data sar_pdata;
extern struct g_sensor_platform_data gsensor_data;
extern struct gyro_platform_data gyro_data;
extern struct ps_platform_data ps_data;
extern volatile int vibrator_shake;

int *get_fingersense_enabled(void)
{
	return &fingersense_enabled;
}

uint8_t *get_hp_offset(void)
{
	return g_hp_offset;
}

int get_stop_auto_als(void)
{
	return g_stop_auto_als;
}

int get_stop_auto_ps(void)
{
	return g_stop_auto_ps;
}

static ssize_t show_sensor_list_info(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int i;
	uint16_t *sensorlist = get_sensorlist();

	hwlog_info("sensor list: ");
	for (i = 0; i <= sensorlist[0]; i++)
		hwlog_info(" %d  ", sensorlist[i]);
	hwlog_info("\n");
	if (memcpy_s(buf, MAX_STR_SIZE, sensorlist,
		((sensorlist[0] + 1) * sizeof(uint16_t))) != EOK)
		return -1;
	return (sensorlist[0] + 1) * sizeof(uint16_t);
}
static DEVICE_ATTR(sensor_list_info, S_IRUGO, show_sensor_list_info, NULL);

#define SENSOR_SHOW_INFO(TAG) \
static ssize_t sensor_show_##TAG##_info(struct device *dev, struct device_attribute *attr, char *buf) \
{ \
	return snprintf_s(buf, MAX_STR_SIZE, MAX_STR_SIZE - 1, "%s\n", get_sensor_chip_info_address(TAG)); \
}

SENSOR_SHOW_INFO(PS);
static DEVICE_ATTR(ps_info, S_IRUGO, sensor_show_PS_info, NULL);

SENSOR_SHOW_INFO(ALS);
static DEVICE_ATTR(als_info, S_IRUGO, sensor_show_ALS_info, NULL);

SENSOR_SHOW_INFO(GYRO);
static DEVICE_ATTR(gyro_info, S_IRUGO, sensor_show_GYRO_info, NULL);

SENSOR_SHOW_INFO(MAG);
static DEVICE_ATTR(mag_info, S_IRUGO, sensor_show_MAG_info, NULL);

SENSOR_SHOW_INFO(ACC);
static DEVICE_ATTR(acc_info, S_IRUGO, sensor_show_ACC_info, NULL);

SENSOR_SHOW_INFO(AIRPRESS);
static DEVICE_ATTR(airpress_info, S_IRUGO, sensor_show_AIRPRESS_info, NULL);

SENSOR_SHOW_INFO(HANDPRESS);
static DEVICE_ATTR(handpress_info, S_IRUGO, sensor_show_HANDPRESS_info, NULL);

#define SENSOR_SHOW_VALUE(TAG) \
static ssize_t sensor_show_##TAG##_read_data(struct device *dev, struct device_attribute *attr, char *buf) \
{ \
	return snprintf_s(buf, MAX_STR_SIZE, MAX_STR_SIZE - 1, \
		"%d\n", get_sensor_read_number(TAG)); \
}

SENSOR_SHOW_VALUE(TAG_ACCEL);
static DEVICE_ATTR(acc_read_data, 0664, sensor_show_TAG_ACCEL_read_data, NULL);

SENSOR_SHOW_VALUE(TAG_MAG);
static DEVICE_ATTR(mag_read_data, 0664, sensor_show_TAG_MAG_read_data, NULL);

SENSOR_SHOW_VALUE(TAG_GYRO);
static DEVICE_ATTR(gyro_read_data, 0664, sensor_show_TAG_GYRO_read_data, NULL);

SENSOR_SHOW_VALUE(TAG_ALS);
static DEVICE_ATTR(als_read_data, 0664, sensor_show_TAG_ALS_read_data, NULL);

SENSOR_SHOW_VALUE(TAG_PS);
static DEVICE_ATTR(ps_read_data, 0664, sensor_show_TAG_PS_read_data, NULL);

SENSOR_SHOW_VALUE(TAG_PRESSURE);
static DEVICE_ATTR(airpress_read_data, 0664, sensor_show_TAG_PRESSURE_read_data, NULL);

SENSOR_SHOW_VALUE(TAG_HANDPRESS);
static DEVICE_ATTR(handpress_read_data, 0664, sensor_show_TAG_HANDPRESS_read_data, NULL);

#define SHOW_SELFTEST_RESULT(TAG) \
static ssize_t show_##TAG##_selfTest_result(struct device *dev, struct device_attribute *attr, char *buf) \
{ \
	return snprintf(buf, MAX_STR_SIZE, "%s\n", sensor_status.TAG##_selftest_result); \
}

SHOW_SELFTEST_RESULT(gyro);
SHOW_SELFTEST_RESULT(mag);
SHOW_SELFTEST_RESULT(accel);
SHOW_SELFTEST_RESULT(gps_4774_i2c);

static ssize_t show_handpress_selfTest_result(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int result;

	if (strncmp(sensor_status.handpress_selftest_result, "1", strlen("1")))
		result = 0;
	else
		result = 1;
	return snprintf(buf, MAX_STR_SIZE, "%d\n", result);
}

#ifdef CONFIG_HUAWEI_DSM
#define SET_SENSOR_SELFTEST(TAGUP, TAGLOW) \
static ssize_t attr_set_##TAGLOW##_selftest(struct device *dev, struct device_attribute *attr, const char *buf, size_t size) \
{ \
	unsigned long val = 0; \
	int err = -1; \
	struct write_info pkg_ap; \
	struct read_info pkg_mcu; \
	uint32_t subcmd; \
	memset(&pkg_ap, 0, sizeof(pkg_ap)); \
	memset(&pkg_mcu, 0, sizeof(pkg_mcu)); \
	if (strict_strtoul(buf, 10, &val)) \
		return -EINVAL; \
	if (val == 1) { \
		pkg_ap.tag = TAG_##TAGUP; \
		pkg_ap.cmd = CMD_CMN_CONFIG_REQ; \
		subcmd = SUB_CMD_SELFTEST_REQ; \
		pkg_ap.wr_buf = &subcmd; \
		pkg_ap.wr_len = SUBCMD_LEN; \
		err = write_customize_cmd(&pkg_ap,  &pkg_mcu, true); \
		if (err) { \
			hwlog_err("send %s selftest cmd to mcu fail,ret=%d\n", #TAGUP, err); \
			memcpy(sensor_status.TAGLOW##_selftest_result, "0", 2); \
			return size; \
		} \
		if (pkg_mcu.errno != 0) { \
			hwlog_err("%s selftest fail\n", #TAGUP); \
			memcpy(sensor_status.TAGLOW##_selftest_result, "0", 2); \
		} else { \
			hwlog_info("%s selftest  success, data len=%d\n", #TAGUP, pkg_mcu.data_length); \
			memcpy(sensor_status.TAGLOW##_selftest_result, "1", 2); \
		} \
	} \
	return size; \
}
#else
#define SET_SENSOR_SELFTEST(TAGUP, TAGLOW) \
static ssize_t attr_set_##TAGLOW##_selftest(struct device *dev, struct device_attribute *attr, const char *buf, size_t size) \
{ \
	unsigned long val = 0; \
	int err = -1; \
	struct write_info pkg_ap; \
	struct read_info pkg_mcu; \
	memset(&pkg_ap, 0, sizeof(pkg_ap)); \
	memset(&pkg_mcu, 0, sizeof(pkg_mcu)); \
	if (strict_strtoul(buf, 10, &val)) \
		return -EINVAL; \
	if (val == 1) { \
		pkg_ap.tag = TAG_##TAGUP; \
		pkg_ap.cmd = CMD_##TAGUP##_SELFTEST_REQ; \
		pkg_ap.wr_buf = NULL; \
		pkg_ap.wr_len = 0; \
		err = write_customize_cmd(&pkg_ap,  &pkg_mcu, true); \
		if (err) { \
			hwlog_err("send %s selftest cmd to mcu fail,ret=%d\n", #TAGUP, err); \
			memcpy(sensor_status.TAGLOW##_selftest_result, "0", 2); \
			return size; \
		} \
		if (pkg_mcu.errno != 0) { \
			hwlog_err("%s selftest fail\n", #TAGUP); \
			memcpy(sensor_status.TAGLOW##_selftest_result, "0", 2); \
		} else { \
			hwlog_info("%s selftest  success, data len=%d\n", #TAGUP, pkg_mcu.data_length); \
			memcpy(sensor_status.TAGLOW##_selftest_result, "1", 2); \
		} \
	} \
	return size; \
}
#endif

SET_SENSOR_SELFTEST(GYRO, gyro);
static DEVICE_ATTR(gyro_selfTest, 0664, show_gyro_selfTest_result,
	attr_set_gyro_selftest);

SET_SENSOR_SELFTEST(MAG, mag);
static DEVICE_ATTR(mag_selfTest, 0664, show_mag_selfTest_result,
	attr_set_mag_selftest);

SET_SENSOR_SELFTEST(ACCEL, accel);
static DEVICE_ATTR(acc_selfTest, 0664, show_accel_selfTest_result,
	attr_set_accel_selftest);

SET_SENSOR_SELFTEST(GPS_4774_I2C, gps_4774_i2c);
static DEVICE_ATTR(gps_4774_i2c_selfTest, 0664,
	show_gps_4774_i2c_selfTest_result, attr_set_gps_4774_i2c_selftest);

SET_SENSOR_SELFTEST(HANDPRESS, handpress);
static DEVICE_ATTR(handpress_selfTest, 0664, show_handpress_selfTest_result,
	attr_set_handpress_selftest);

#ifdef SENSOR_DATA_ACQUISITION
static int init_msg_for_enq(struct message *msg)
{
	if (!msg)
		return -1;

	memset(msg, 0, sizeof(struct message));
	msg->data_source = DATA_FROM_KERNEL;
	msg->num_events = 0;
	msg->version = 1;
	return 0;
}

static int init_event_of_msg(struct event *events, struct sensor_eng_cal_test sensor_test)
{
	if (!events)
		return -1;
	memset(events, 0, sizeof(struct event));
	events->error_code = 0;
	events->cycle = 0;
	memcpy(events->station, NA, sizeof(NA));
	memcpy(events->device_name, sensor_test.name, sizeof(sensor_test.name));
	memcpy(events->bsn, NA, sizeof(NA));
	memcpy(events->min_threshold, NA, sizeof(NA));
	memcpy(events->max_threshold, NA, sizeof(NA));
	memcpy(events->result, sensor_test.result, sizeof(sensor_test.result));
	memcpy(events->firmware, NA, sizeof(NA));
	memcpy(events->description, NA, sizeof(NA));
	return 0;
}
static int enq_msg_data_in_sensorhub_single(struct event events)
{
	struct message *msg = NULL;
	int ret = -1;
	struct dsm_client *client = inputhub_get_shb_dclient();

	msg = (struct message *)kzalloc(sizeof(struct message), GFP_KERNEL);
	if (!msg) {
		hwlog_err("alloc message failed\n");
		return ret;
	}

	msg->data_source = DATA_FROM_KERNEL;
	msg->num_events = 1;
	msg->version = 1;
	events.error_code = 0;
	events.cycle = 0;
	memcpy(events.station, NA, sizeof(NA));
	memcpy(events.bsn, NA, sizeof(NA));
	memcpy(events.firmware, NA, sizeof(NA));
	memcpy(events.description, NA, sizeof(NA));
	memcpy(&(msg->events[0]), &events, sizeof(events));

	if (!dsm_client_ocuppy(client)) {
		ret = dsm_client_copy_ext(client, msg, sizeof(struct message));
		if (ret > 0)
			dsm_client_notify(client, DA_SENSOR_HUB_ERROR_NO);
		else
			hwlog_err("dsm_client_notify failed!");
	}

	kfree(msg);
	return ret;
}

static void msg_process_for_enq(struct message *msg,
	const struct sensor_eng_cal_test *sensor_test, int num, int cal_value)
{
	if (snprintf_s(msg->events[num].value, MAX_VAL_LEN, MAX_VAL_LEN - 1,
		"%d", *(sensor_test->cal_value + cal_value)) < 0)
		hwlog_err("%s snprintf_s failed\n", __func__);
	if (cal_value < sensor_test->threshold_num) {
		if (snprintf_s(msg->events[num].min_threshold,
			MAX_VAL_LEN, MAX_VAL_LEN - 1, "%d",
			*(sensor_test->min_threshold + cal_value)) < 0)
			hwlog_err("snprintf_s failed\n");
		if (snprintf_s(msg->events[num].max_threshold,
			MAX_VAL_LEN, MAX_VAL_LEN - 1, "%d",
			*(sensor_test->max_threshold + cal_value)) < 0)
			hwlog_err("snprintf_s failed\n");
	}
	if (memcpy_s(msg->events[num].test_name,
		sizeof(msg->events[num].test_name),
		sensor_test->test_name[cal_value],
		strlen(sensor_test->test_name[cal_value]) + 1) != EOK)
		hwlog_err("memcpy_s error\n");
	msg->events[num].item_id = sensor_test->first_item + cal_value;
}

static int enq_msg_data_in_sensorhub(struct sensor_eng_cal_test sensor_test)
{
	struct message *msg = NULL;
	int ret = -1;
	int cal_value = 0;
	int i;
	struct dsm_client *client = inputhub_get_shb_dclient();

	if (sensor_test.value_num > MAX_COPY_EVENT_SIZE || !sensor_test.cal_value) {
		hwlog_info("%s bad value!!\n", __func__);
		return ret;
	}

	while (cal_value < sensor_test.value_num) {
		msg = (struct message *)kzalloc(sizeof(struct message), GFP_KERNEL);
		ret = init_msg_for_enq(msg);
		if (ret) {
			hwlog_err("alloc mesage failed\n");
			return ret;
		}

		for (i = 0; i < MAX_MSG_EVENT_NUM &&
			cal_value < sensor_test.value_num; i++, cal_value++) {
			ret =  init_event_of_msg(&(msg->events[i]), sensor_test);
			if (ret) {
				hwlog_err("init_event_of_msg failed\n");
				kfree(msg);
				return ret;
			}
			msg_process_for_enq(msg, &sensor_test, i, cal_value);
		}
		msg->num_events = i;
		ret = dsm_client_copy_ext(client, msg, sizeof(struct message));
		if (ret <= 0) {
			ret = -1;
			hwlog_err("%s dsm_client_copy_ext for sensor failed\n", sensor_test.name);
			kfree(msg);
			return ret;
		} else {
			ret = 0;
			kfree(msg);
		}
	}

	hwlog_info("%s ,%s succ!!\n", sensor_test.name, __func__);
	return ret;
}


static void enq_notify_work_sensor(struct sensor_eng_cal_test sensor_test)
{
	int ret;
	struct dsm_client *client = inputhub_get_shb_dclient();

	if (!dsm_client_ocuppy(client)) {
		ret = enq_msg_data_in_sensorhub(sensor_test);
		if (!ret)
			dsm_client_notify(client, DA_SENSOR_HUB_ERROR_NO);
	}
}

static void cap_prox_enq_notify_work(const int item_id, uint16_t value,
	uint16_t min_threshold, uint16_t max_threshold, const char *test_name)
{
	int ret;
	struct event cap_prox_event;

	memset(&cap_prox_event, 0, sizeof(cap_prox_event));

	if (!test_name)
		return;
	cap_prox_event.item_id = item_id;
	memcpy(cap_prox_event.device_name, CAP_PROX_TEST_CAL, sizeof(CAP_PROX_TEST_CAL));
	memcpy(cap_prox_event.result, CAP_PROX_RESULT, (strlen(CAP_PROX_RESULT) + 1));
	memcpy(cap_prox_event.test_name, test_name, (strlen(test_name) + 1));
	snprintf(cap_prox_event.value, MAX_VAL_LEN, "%d", value);
	snprintf(cap_prox_event.min_threshold, MAX_VAL_LEN, "%u", min_threshold);
	snprintf(cap_prox_event.max_threshold, MAX_VAL_LEN, "%u", max_threshold);

	ret = enq_msg_data_in_sensorhub_single(cap_prox_event);
	if (ret > 0)
		hwlog_info("%s succ!!item_id=%d\n", __func__, cap_prox_event.item_id);
	else
		hwlog_info("%s failed!!\n", __func__);
}

static void cap_prox_calibrate_data_process(int calibrate_index)
{
	struct sensor_eng_cal_test cap_prox_test = { 0 };
	int32_t min_threshold[CAP_PROX_THRESHOLD_NUM] = { 0 };
	int32_t max_threshold[CAP_PROX_THRESHOLD_NUM] = { 0 };
	int32_t cap_prox_data[CAP_PROX_CAL_NUM] = { 0 };
	int i;

	cap_prox_test.cal_value = cap_prox_data;
	cap_prox_test.value_num = CAP_PROX_CAL_NUM;
	cap_prox_test.threshold_num = CAP_PROX_THRESHOLD_NUM;
	cap_prox_test.min_threshold = min_threshold;
	cap_prox_test.max_threshold = max_threshold;
	if (memcpy_s(cap_prox_test.name, sizeof(cap_prox_test.name),
		CAP_PROX_TEST_CAL, sizeof(CAP_PROX_TEST_CAL)) != EOK) {
		hwlog_err("cap_prox calibrate process memcpy name error\n");
		return;
	}
	if (memcpy_s(cap_prox_test.result, sizeof(cap_prox_test.result),
		CAP_PROX_RESULT, strlen(CAP_PROX_RESULT) + 1) != EOK) {
		hwlog_err("cap_prox calibrate process memcpy result error\n");
		return;
	}

	switch (calibrate_index) {
	case 1: /* 1: near data */
		cap_prox_data[0] = sar_calibrate_datas.abov_cali_data.diff[0];
		cap_prox_data[1] = sar_calibrate_datas.abov_cali_data.diff[1];
		min_threshold[0] = sar_pdata.sar_datas.abov_data.calibrate_thred[DIFF_MIN_THREDHOLD];
		max_threshold[0] = sar_pdata.sar_datas.abov_data.calibrate_thred[DIFF_MAX_THREDHOLD];
		cap_prox_test.first_item = SAR_SENSOR_DIFF1_MSG;
		for (i = 0; i < CAP_PROX_CAL_NUM; i++)
			cap_prox_test.test_name[i] = cap_prox_diff_test_name[i];

		enq_notify_work_sensor(cap_prox_test);
		break;
	case 2: /* 2: far data */
		cap_prox_data[0] = sar_calibrate_datas.abov_cali_data.offset[0];
		cap_prox_data[1] = sar_calibrate_datas.abov_cali_data.offset[1];
		min_threshold[0] = sar_pdata.sar_datas.abov_data.calibrate_thred[OFFSET_MIN_THREDHOLD];
		max_threshold[0] = sar_pdata.sar_datas.abov_data.calibrate_thred[OFFSET_MAX_THREDHOLD];
		cap_prox_test.first_item = SAR_SENSOR_PH1_OFFSET_MSG;
		for (i = 0; i < CAP_PROX_CAL_NUM; i++)
			cap_prox_test.test_name[i] = cap_prox_offset_test_name[i];

		enq_notify_work_sensor(cap_prox_test);
		break;
	default:
		hwlog_err("%s: error calibrate_index = %d\n", __func__, calibrate_index);
		break;
	}
}

static void cap_prox_do_enq_work(int calibrate_index)
{
	uint16_t diff;
	uint16_t offset;
	uint16_t *calibrate_thred = NULL;
	char *chip_info = get_sensor_chip_info_address(CAP_PROX);

	if (!strncmp(chip_info, "huawei,semtech-sx9323", strlen("huawei,semtech-sx9323"))) {
		diff = sar_calibrate_datas.semtech_cali_data.diff;
		offset = sar_calibrate_datas.semtech_cali_data.offset;
		calibrate_thred = sar_pdata.sar_datas.semteck_data.calibrate_thred;
	} else if (!strncmp(chip_info, "huawei,abov-a96t3x6", strlen("huawei,abov-a96t3x6"))) {
		cap_prox_calibrate_data_process(calibrate_index);
		return; /* avoid fall through */
	} else if (!strncmp(chip_info, "huawei,awi-aw9610x", strlen("huawei,awi-aw9610x"))) {
		diff = sar_calibrate_datas.aw9610_cali_data.diff;
		offset = sar_calibrate_datas.aw9610_cali_data.offset;
		calibrate_thred = sar_pdata.sar_datas.aw9610_data.calibrate_thred;
	}

	if (calibrate_thred) {
		switch (calibrate_index) {
		case 1: /* 1: near data */
			cap_prox_enq_notify_work(SAR_SENSOR_DIFF_MSG, diff,
						calibrate_thred[DIFF_MIN_THREDHOLD],
						calibrate_thred[DIFF_MAX_THREDHOLD],
						CAP_PROX_DIFF);
			break;
		case 2: /* 2: far data */
			cap_prox_enq_notify_work(SAR_SENSOR_OFFSET_MSG, offset,
						calibrate_thred[OFFSET_MIN_THREDHOLD],
						calibrate_thred[OFFSET_MAX_THREDHOLD],
						CAP_PROX_OFFSET);
			break;
		default:
			break;
		}
	}
}
#endif
static ssize_t i2c_rw_pi(struct device *dev, struct device_attribute *attr,
	const char *buf, size_t count)
{
	uint64_t val = 0;
	int ret;
	uint8_t bus_num;
	uint8_t i2c_address;
	uint8_t reg_add;
	uint8_t len;
	uint8_t rw;
	uint8_t buf_temp[DEBUG_DATA_LENGTH] = { 0 };

	if (strict_strtoull(buf, TO_HEXADECIMAL, &val))
		return -EINVAL;
	bus_num = (val >> 40) & 0xff;
	i2c_address = (val >> 32) & 0xff;
	reg_add = (val >> 24) & 0xff;
	len = (val >> 16) & 0xff;
	if (len > DEBUG_DATA_LENGTH - 1) {
		hwlog_err("len exceed %d\n", len);
		len = DEBUG_DATA_LENGTH - 1;
	}
	rw = (val >> 8) & 0xff;
	buf_temp[0] = reg_add;
	buf_temp[1] = (uint8_t)(val & 0xff);

	hwlog_info("In %s! bus_num = %d, i2c_address = %d, reg_add = %d, len = %d, rw = %d, buf_temp[1] = %d\n",
		__func__, bus_num, i2c_address, reg_add, len, rw, buf_temp[1]);
	if (rw)
		ret = mcu_i2c_rw(bus_num, i2c_address, &buf_temp[0], 1, &buf_temp[1], len);
	else
		ret = mcu_i2c_rw(bus_num, i2c_address, buf_temp, 2, NULL, 0); /* 2 bytes */

	if (ret < 0)
		hwlog_err("oper %d(1/32:r 0/31:w) i2c reg fail!\n", rw);
	if (rw) {
		hwlog_err("i2c reg %x value %x %x %x %x\n", reg_add, buf_temp[1],
			buf_temp[2], buf_temp[3], buf_temp[4]);
		memcpy(debug_read_data_buf, &buf_temp[1], len);
	}
	return count;
}

static ssize_t i2c_rw_pi_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	unsigned int i;
	unsigned int len = 0;
	char *p = buf;

	if (!buf)
		return -1;

	for (i = 0; i < DEBUG_DATA_LENGTH; i++) {
		/* len 6 enough */
		snprintf(p, 6, "0x%x,", debug_read_data_buf[i]);
		if (debug_read_data_buf[i] > 0xf) {
			p += 5; /* 5 bytes */
			len += 5; /* 5 bytes */
		} else {
			p += 4; /* 4 bytes */
			len += 4; /* 4 bytes */
		}
	}

	p = buf;
	*(p + len - 1) = 0;

	p = NULL;
	return len;
}

static DEVICE_ATTR(i2c_rw, 0664, i2c_rw_pi_show, i2c_rw_pi);

static ssize_t i2c_rw16_pi(struct device *dev, struct device_attribute *attr,
	const char *buf, size_t count)
{
	uint64_t val = 0;
	int ret;
	uint8_t bus_num;
	uint8_t i2c_address;
	uint8_t reg_add;
	uint8_t len;
	uint8_t rw;
	uint8_t buf_temp[3] = { 0 };

	if (strict_strtoull(buf, TO_HEXADECIMAL, &val))
		return -EINVAL;
	bus_num = (val >> 48) & 0xff;
	i2c_address = (val >> 40) & 0xff;
	reg_add = (val >> 32) & 0xff;
	len = (val >> 24) & 0xff;
	if (len > 2) { /* len max 2 */
		hwlog_err("len exceed %d\n", len);
		len = 2; /* len max 2 */
	}
	rw = (val >> 16) & 0xff;
	buf_temp[0] = reg_add;
	buf_temp[1] = (uint8_t)(val >> 8);
	buf_temp[2] = (uint8_t)(val & 0xff);

	hwlog_info("In %s! bus_num=%d, i2c_address=%d, reg_add=%d, len=%d, rw=%d, buf_temp[1]=0x%02x, buf_temp[2]=0x%02x\n",
		__func__, bus_num, i2c_address, reg_add, len, rw, buf_temp[1], buf_temp[2]);
	if (rw)
		ret = mcu_i2c_rw(bus_num, i2c_address, buf_temp, 1, &buf_temp[1], (uint32_t)len);
	else
		ret = mcu_i2c_rw(bus_num, i2c_address, buf_temp, 1 + len, NULL, 0);

	if (ret < 0)
		hwlog_err("oper %d(1:r 0:w) i2c reg fail!\n", rw);
	if (rw) {
		hwlog_err("i2c reg %x value %x %x\n", reg_add, buf_temp[1], buf_temp[2]);
		/* copy 2 bytes */
		memcpy(i2c_rw16_data_buf, &buf_temp[1], 2);
	}
	return count;
}

static ssize_t i2c_rw16_pi_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	char *p = buf;

	if (!buf)
		return -1;

	/* 8 bytes */
	snprintf(p, 8, "0x%02x%02x\n", i2c_rw16_data_buf[0], i2c_rw16_data_buf[1]);
	*(p + 7) = 0; /* 7: terminator 0 */

	p = NULL;
	return 8; /* 8 bytes */
}
static DEVICE_ATTR(i2c_rw16, 0664, i2c_rw16_pi_show, i2c_rw16_pi);

static void save_to_file(const char *file_path, const char *content)
{
	static mm_segment_t oldfs;
	struct file *fp = NULL;

	oldfs = get_fs();
	set_fs(KERNEL_DS);

	/* 0644: rights */
	fp = filp_open(file_path, O_WRONLY | O_APPEND, 0644);
	if (IS_ERR(fp)) {
		hwlog_err("oper %s fail\n", file_path);
		set_fs(oldfs);
		return;
	}
	vfs_write(fp, content, strlen(content), &(fp->f_pos));
	filp_close(fp, NULL);
	set_fs(oldfs);
}

static char *get_cali_error_code(int error_code)
{
	if ((error_code > 0) && (error_code < RET_TYPE_MAX))
		return cali_error_code_str[error_code];
	return NULL;
}

static void get_test_time(char *date_str, size_t size)
{
	struct timeval time;
	unsigned long local_time;
	struct rtc_time tm;

	do_gettimeofday(&time);
	/* 60 sec per minute */
	local_time = (u32)(time.tv_sec - (sys_tz.tz_minuteswest * 60));
	rtc_time_to_tm(local_time, &tm);

	if (snprintf_s(date_str, CLI_TIME_STR_LEN, CLI_TIME_STR_LEN - 1,
		"%04d-%02d-%02d %02d:%02d:%02d",
		tm.tm_year + 1900, tm.tm_mon + 1, /* tm_year start from 1900 */
		tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec) < 0)
		return;
}

static ssize_t attr_acc_calibrate_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int val = return_calibration;

	hwlog_info("acc_calibrate_show is old way,result=%d\n", val);
	return snprintf(buf, PAGE_SIZE, "%d\n", val);
}

static int acc_calibrate_save(const void *buf, int length)
{
	const int32_t *poffset_data = (const int32_t *)buf;
	int ret;
	int i;

	if (!buf || length <= 0) {
		hwlog_err("%s invalid argument", __func__);
		return_calibration = EXEC_FAIL;
		return -1;
	}
	hwlog_info("%s:gsensor calibrate ok, %d  %d  %d  %d  %d  %d %d  %d  %d %d  %d  %d %d  %d  %d\n",
		__func__, *poffset_data, *(poffset_data + 1), *(poffset_data + 2),
		*(poffset_data + 3), *(poffset_data + 4), *(poffset_data + 5),
		*(poffset_data + 6), *(poffset_data + 7), *(poffset_data + 8),
		*(poffset_data + 9), *(poffset_data + 10), *(poffset_data + 11),
		*(poffset_data + 12), *(poffset_data + 13), *(poffset_data + 14));

	for (i = 0; i < ACC_GYRO_OFFSET_CALIBRATE_LENGTH; i++) {
		if (*(poffset_data + i) < acc_calib_threshold[i].low_threshold ||
			*(poffset_data + i) > acc_calib_threshold[i].high_threshold) {
			hwlog_err("%s: acc calibrated_data is out of range. i = %d, num = %d.\n",
				__func__, i, *(poffset_data + i));
			return_calibration = NV_FAIL;
			return -1;
		}
	}

	ret = write_gsensor_offset_to_nv((char *)buf, length);
	if (ret) {
		hwlog_err("nv write fail.\n");
		return_calibration = NV_FAIL;
		return -1;
	}
	return_calibration = SUC;
	return 0;
}

static struct read_info send_calibrate_cmd(uint8_t tag, unsigned long val, enum ret_type *rtype)
{
	int ret;
	struct write_info pkg_ap = { 0 };
	struct read_info pkg_mcu = { 0 };
	pkt_parameter_req_t spkt;
	struct pkt_header *shd = (struct pkt_header *)&spkt;

	pkg_ap.tag = tag;
	pkg_ap.cmd = CMD_CMN_CONFIG_REQ;
	spkt.subcmd = SUB_CMD_SELFCALI_REQ;
	pkg_ap.wr_buf = &shd[1];
	pkg_ap.wr_len = sizeof(val) + SUBCMD_LEN;
	memcpy(spkt.para, &val, sizeof(val));
	hwlog_err("tag %d calibrator val is %lu  len is %lu.\n", tag, val, sizeof(val));
	ret = write_customize_cmd(&pkg_ap, &pkg_mcu, true);
	if (ret) {
		*rtype = COMMU_FAIL;
		hwlog_err("send tag %d calibrate cmd to mcu fail,ret=%d\n", tag, ret);
	} else if (pkg_mcu.errno != 0) {
		hwlog_err("send tag %d calibrate fail, %d\n", tag, pkg_mcu.errno);
		*rtype = EXEC_FAIL;
	} else {
		hwlog_info("send tag %d calibrate  success, data len=%d\n", tag, pkg_mcu.data_length);
		*rtype = SUC;
	}
	return pkg_mcu;
}

struct read_info send_airpress_calibrate_cmd(uint8_t tag, unsigned long val, enum ret_type *rtype)
{
	return send_calibrate_cmd(tag, val, rtype);
}

static const char *acc_calibrate_param[] = {
	ACC_CALI_X_OFFSET, ACC_CALI_Y_OFFSET, ACC_CALI_Z_OFFSET,
	ACC_CALI_X_SEN, ACC_CALI_Y_SEN, ACC_CALI_Z_SEN, ACC_CALI_XIS_00,
	ACC_CALI_XIS_01, ACC_CALI_XIS_02, ACC_CALI_XIS_10, ACC_CALI_XIS_11,
	ACC_CALI_XIS_12, ACC_CALI_XIS_20, ACC_CALI_XIS_21, ACC_CALI_XIS_22,
};

static ssize_t attr_acc_calibrate_write(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned long val = 0;
	int ret;
	int i;
	char content[CLI_CONTENT_LEN_MAX] = { 0 };
	const int32_t *acc_cali_data = NULL;
	char date_str[CLI_TIME_STR_LEN] = { 0 };
	interval_param_t param;
	struct read_info read_pkg;
#ifdef SENSOR_DATA_ACQUISITION
	int32_t min_threshold_acc[ACC_THRESHOLD_NUM] = {
		-gsensor_data.x_calibrate_thredhold,
		-gsensor_data.y_calibrate_thredhold,
		-gsensor_data.z_calibrate_thredhold
	};
	int32_t max_threshold_acc[ACC_THRESHOLD_NUM] = {
		gsensor_data.x_calibrate_thredhold,
		gsensor_data.y_calibrate_thredhold,
		gsensor_data.z_calibrate_thredhold
	};
	struct sensor_eng_cal_test acc_test = { 0 };
#endif

	if (strict_strtoul(buf, TO_DECIMALISM, &val))
		return -EINVAL;

	if (val < 1 || val > 6) /* max 6 */
		return count;

	if (sensor_status.opened[TAG_ACCEL] == 0) { /* if acc is not opened, open first */
		acc_close_after_calibrate = true;
		hwlog_info("send acc open cmd(during calibrate) to mcu.\n");
		ret = inputhub_sensor_enable(TAG_ACCEL, true);
		if (ret) {
			return_calibration = COMMU_FAIL;
			hwlog_err("send acc open cmd(during calibrate) to mcu fail,ret=%d\n", ret);
			return count;
		}
	} else {
		acc_close_after_calibrate = false;
	}
	/* period must <= 100 ms */
	if ((sensor_status.delay[TAG_ACCEL] == 0) ||
		(sensor_status.delay[TAG_ACCEL] > ACC_PERIOD_MAX)) {
		hwlog_info("send acc setdelay cmd(during calibrate) to mcu.\n");
		memset(&param, 0, sizeof(param));
		param.period = ACC_PERIOD_MAX;
		ret = inputhub_sensor_setdelay(TAG_ACCEL, &param);
		if (ret) {
			return_calibration = COMMU_FAIL;
			hwlog_err("send acc set delay cmd(during calibrate) to mcu fail,ret=%d\n", ret);
			return count;
		}
	}
	msleep(300); /* sleep 300 ms */
	/* send calibrate command, need set delay first */
	read_pkg = send_calibrate_cmd(TAG_ACCEL, val, &return_calibration);
	if (return_calibration == COMMU_FAIL)
		return count;
	else if (read_pkg.errno == 0)
		acc_calibrate_save(read_pkg.data, read_pkg.data_length);
	get_test_time(date_str, sizeof(date_str));
	acc_cali_data = (const int32_t *)read_pkg.data;

#ifdef SENSOR_DATA_ACQUISITION
	acc_test.cal_value = (int32_t *)acc_cali_data;
	acc_test.value_num = ACC_CAL_NUM;
	acc_test.threshold_num = ACC_THRESHOLD_NUM;
	acc_test.first_item = ACC_CALI_X_OFFSET_MSG;
	acc_test.min_threshold = (int32_t *)min_threshold_acc;
	acc_test.max_threshold = (int32_t *)max_threshold_acc;
	memcpy(acc_test.name, ACC_TEST_CAL, sizeof(ACC_TEST_CAL));
	memcpy(acc_test.result, ACC_CAL_RESULT, (strlen(ACC_CAL_RESULT) + 1));
	for (i = 0; i < ACC_CAL_NUM; i++)
		acc_test.test_name[i] = acc_test_name[i];
	enq_notify_work_sensor(acc_test);
#endif

	for (i = 0; i < ARRAY_SIZE(acc_calibrate_param); i++) {
		memset(&content, 0, sizeof(content));
		snprintf(content, CLI_CONTENT_LEN_MAX, acc_calibrate_param[i],
			*(acc_cali_data + i), ((return_calibration == SUC) ? "pass" : "fail"),
			(int)val, get_cali_error_code(return_calibration), date_str);
		save_to_file(DATA_CLLCT, content);
	}

	if (acc_close_after_calibrate == true) {
		acc_close_after_calibrate = false;
		hwlog_info("send acc close cmd(during calibrate) to mcu.\n");
		ret = inputhub_sensor_enable(TAG_ACCEL, false);
		if (ret) {
			return_calibration = COMMU_FAIL;
			hwlog_err("send acc close cmd(during calibrate) to mcu fail,ret=%d\n", ret);
			return count;
		}
	}
	return count;
}

static DEVICE_ATTR(acc_calibrate, 0664, attr_acc_calibrate_show, attr_acc_calibrate_write);

static ssize_t attr_gyro_calibrate_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int val = gyro_calibration_res;

	hwlog_info("gyro_calibrate_show is old way,result=%d\n", val);
	return snprintf(buf, PAGE_SIZE, "%d\n", val);
}

static int gyro_calibrate_save(const void *buf, int length)
{
	const int32_t *poffset_data = (const int32_t *)buf;
	int ret;
	int gyro_range_factor = 1;
	int i;

	if (!buf || length <= 0) {
		hwlog_err("%s invalid argument", __func__);
		gyro_calibration_res = EXEC_FAIL;
		return -1;
	}
	hwlog_info("%s:gyro_sensor calibrate ok, %d  %d  %d %d  %d  %d %d  %d  %d %d  %d  %d  %d  %d  %d\n",
		__func__, *poffset_data, *(poffset_data + 1), *(poffset_data + 2),
		*(poffset_data + 3), *(poffset_data + 4), *(poffset_data + 5),
		*(poffset_data + 6), *(poffset_data + 7), *(poffset_data + 8),
		*(poffset_data + 9), *(poffset_data + 10), *(poffset_data + 11),
		*(poffset_data + 12), *(poffset_data + 13), *(poffset_data + 14));

	if (gyro_range == GYRO_RANGE_1000DPS)
		gyro_range_factor = GYRO_RANGE_FROM_2000DPS_TO_1000DPS;
	for (i = 0; i < ACC_GYRO_OFFSET_CALIBRATE_LENGTH; i++) {
		if (*(poffset_data + i) < gyro_calib_threshold[i].low_threshold * gyro_range_factor ||
			*(poffset_data + i) > gyro_calib_threshold[i].high_threshold * gyro_range_factor) {
			hwlog_err("%s: gyro calibrated_data is out of range. i = %d, num = %d.\n",
				__func__, i, *(poffset_data + i));
			gyro_calibration_res = NV_FAIL;
			return -1;
		}
	}

	ret = write_gyro_sensor_offset_to_nv((char *)buf, length);
	if (ret) {
		hwlog_err("nv write fail.\n");
		gyro_calibration_res = NV_FAIL;
		return -1;
	}
	gyro_calibration_res = SUC;
	return 0;
}

static const char *gyro_calibrate_param[] = {
	GYRO_CALI_X_OFFSET, GYRO_CALI_Y_OFFSET, GYRO_CALI_Z_OFFSET,
	GYRO_CALI_X_SEN, GYRO_CALI_Y_SEN, GYRO_CALI_Z_SEN, GYRO_CALI_XIS_00,
	GYRO_CALI_XIS_01, GYRO_CALI_XIS_02, GYRO_CALI_XIS_10, GYRO_CALI_XIS_11,
	GYRO_CALI_XIS_12, GYRO_CALI_XIS_20, GYRO_CALI_XIS_21, GYRO_CALI_XIS_22,
};

static ssize_t attr_gyro_calibrate_write(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned long val = 0;
	int ret;
	int i;
	struct read_info read_pkg;
	char content[CLI_CONTENT_LEN_MAX] = { 0 };
	const int32_t *gyro_cali_data = NULL;
	char date_str[CLI_TIME_STR_LEN] = { 0 };
	interval_param_t param;
#ifdef SENSOR_DATA_ACQUISITION
	int32_t min_threshold_gyro[GYRO_THRESHOLD_NUM] = {
		-gyro_data.calibrate_thredhold,
		-gyro_data.calibrate_thredhold,
		-gyro_data.calibrate_thredhold
	}; /* -40dps */
	int32_t max_threshold_gyro[GYRO_THRESHOLD_NUM] = {
		gyro_data.calibrate_thredhold,
		gyro_data.calibrate_thredhold,
		gyro_data.calibrate_thredhold
	}; /* 40dps */
	struct sensor_eng_cal_test gyro_test = { 0 };
#endif

	if (strict_strtoul(buf, TO_DECIMALISM, &val))
		return -EINVAL;

	if ((val < 1) || (val > 7)) { /* max 7 */
		hwlog_err("set gyro calibrate val invalid,val=%lu\n", val);
		return count;
	}

	if (sensor_status.opened[TAG_GYRO] == 0) { /* if gyro is not opened, open first */
		gyro_close_after_calibrate = true;
		hwlog_info("send gyro open cmd(during calibrate) to mcu.\n");
		ret = inputhub_sensor_enable(TAG_GYRO, true);
		if (ret) {
			gyro_calibration_res = COMMU_FAIL;
			hwlog_err("send gyro open cmd(during calibrate) to mcu fail,ret=%d\n", ret);
			return count;
		}
	} else {
		gyro_close_after_calibrate = false;
	}

	if ((sensor_status.delay[TAG_GYRO] == 0) ||
		(sensor_status.delay[TAG_GYRO] > GYRO_PERIOD_MAX)) {
		hwlog_info("send gyro setdelay cmd(during calibrate) to mcu.\n");
		memset(&param, 0, sizeof(param));
		param.period = GYRO_PERIOD_MAX;
		ret = inputhub_sensor_setdelay(TAG_GYRO, &param);
		if (ret) {
			gyro_calibration_res = COMMU_FAIL;
			hwlog_err("send gyro set delay cmd(during calibrate) to mcu fail,ret=%d\n", ret);
			return count;
		}
	}
	msleep(300); /* sleep 300 ms */
	/* send calibrate command, need set delay first */
	read_pkg = send_calibrate_cmd(TAG_GYRO, val, &gyro_calibration_res);
	if (gyro_calibration_res == COMMU_FAIL) {
		return count;
	} else if (read_pkg.errno == 0) {
		gyro_cali_data = (const int32_t *)read_pkg.data;
		if (val == 1) {
			gyro_calib_data[0] = *gyro_cali_data;
			gyro_calib_data[1] = *(gyro_cali_data + 1);
			gyro_calib_data[2] = *(gyro_cali_data + 2);
			gyro_calibrate_save(gyro_calib_data, sizeof(gyro_calib_data));
		} else if (val == GYRO_DYN_CALIBRATE_END_ORDER) {
			gyro_calib_data[3] = *(gyro_cali_data + 3);
			gyro_calib_data[4] = *(gyro_cali_data + 4);
			gyro_calib_data[5] = *(gyro_cali_data + 5);
			gyro_calib_data[6] = *(gyro_cali_data + 6);
			gyro_calib_data[7] = *(gyro_cali_data + 7);
			gyro_calib_data[8] = *(gyro_cali_data + 8);
			gyro_calib_data[9] = *(gyro_cali_data + 9);
			gyro_calib_data[10] = *(gyro_cali_data + 10);
			gyro_calib_data[11] = *(gyro_cali_data + 11);
			gyro_calib_data[12] = *(gyro_cali_data + 12);
			gyro_calib_data[13] = *(gyro_cali_data + 13);
			gyro_calib_data[14] = *(gyro_cali_data + 14);
			gyro_calibrate_save(gyro_calib_data, sizeof(gyro_calib_data));
		} else {
			gyro_calibration_res = SUC;
		}

		hwlog_info("gyro calibrate success, val=%lu data=%d %d %d %d %d %d %d %d %d %d %d %d %d %d %d, len=%d\n",
			val, gyro_calib_data[0], gyro_calib_data[1], gyro_calib_data[2],
			gyro_calib_data[3], gyro_calib_data[4], gyro_calib_data[5],
			gyro_calib_data[6], gyro_calib_data[7], gyro_calib_data[8],
			gyro_calib_data[9], gyro_calib_data[10], gyro_calib_data[11],
			gyro_calib_data[12], gyro_calib_data[13], gyro_calib_data[14],
			read_pkg.data_length);
	}

	if (val == 1 || val == GYRO_DYN_CALIBRATE_END_ORDER) {
		get_test_time(date_str, sizeof(date_str));
#ifdef SENSOR_DATA_ACQUISITION
		gyro_test.cal_value = gyro_calib_data;
		gyro_test.first_item = GYRO_CALI_X_OFFSET_MSG;
		gyro_test.value_num = GYRO_CAL_NUM;
		gyro_test.threshold_num = GYRO_THRESHOLD_NUM;
		gyro_test.min_threshold = (int32_t *)min_threshold_gyro;
		gyro_test.max_threshold = (int32_t *)max_threshold_gyro;
		memcpy(gyro_test.name, GYRO_TEST_CAL, sizeof(GYRO_TEST_CAL));
		memcpy(gyro_test.result, GYRO_CAL_RESULT, (strlen(GYRO_CAL_RESULT) + 1));
		for (i = 0; i < GYRO_CAL_NUM; i++)
			gyro_test.test_name[i] = gyro_test_name[i];
		enq_notify_work_sensor(gyro_test);
#endif

		for (i = 0; i < ARRAY_SIZE(gyro_calibrate_param); i++) {
			memset(&content, 0, sizeof(content));
			snprintf(content, CLI_CONTENT_LEN_MAX, gyro_calibrate_param[i],
				gyro_calib_data[i], ((gyro_calibration_res == SUC) ? "pass" : "fail"),
				(int)val, get_cali_error_code(gyro_calibration_res), date_str);
			save_to_file(DATA_CLLCT, content);
		}
	}

	if (gyro_close_after_calibrate == true) {
		gyro_close_after_calibrate = false;
		hwlog_info("send gyro close cmd(during calibrate) to mcu.\n");
		ret = inputhub_sensor_enable(TAG_GYRO, false);
		if (ret) {
			gyro_calibration_res = COMMU_FAIL;
			hwlog_err("send gyro close cmd(during calibrate) to mcu fail,ret=%d\n", ret);
			return count;
		}
	}
	return count;
}
static DEVICE_ATTR(gyro_calibrate, 0664, attr_gyro_calibrate_show,
	attr_gyro_calibrate_write);

static ssize_t attr_ps_calibrate_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int val = ps_calibration_res;

	return snprintf(buf, PAGE_SIZE, "%d\n", val);
}

static int write_ps_offset_to_nv(int *temp)
{
	const int *poffset_data = (const int *)user_info.nv_data;
	uint8_t *data = get_ps_sensor_calibrate_data();

	if (!temp) {
		hwlog_err("%s fail, invalid para!\n", __func__);
		return -1;
	}

	if (write_calibrate_data_to_nv(PS_CALIDATA_NV_NUM, PS_CALIDATA_NV_SIZE,
		"PSENSOR", (char *)temp))
		return -1;

	hwlog_info("%s temp: %d,%d,%d\n", __func__, temp[0], temp[1], temp[2]);
	if (memcpy_s(data, MAX_SENSOR_CALIBRATE_DATA_LENGTH, temp,
		sizeof(temp[0]) * 3) != EOK) /* multiply 3 */
		return -1;
	hwlog_info("nve_direct_access write temp %d, %d, %d\n", *poffset_data,
		*(poffset_data + 1), *(poffset_data + 2));
	msleep(10); /* sleep 10 ms */
	if (read_calibrate_data_from_nv(PS_CALIDATA_NV_NUM, PS_CALIDATA_NV_SIZE, "PSENSOR"))
		return -1;

	if ((*poffset_data != temp[0]) || (*(poffset_data + 1) != temp[1]) ||
		(*(poffset_data + 2) != temp[2])) {
		hwlog_err("nv write fail, %d, %d, %d\n", *poffset_data,
			*(poffset_data + 1), *(poffset_data + 2));
		return -1;
	}
	return 0;
}

static int write_tof_offset_to_nv(uint8_t *temp)
{
	uint8_t *data = get_tof_sensor_calibrate_data();

	if (!temp) {
		hwlog_err("%s fail, invalid para!\n", __func__);
		return -1;
	}

	if (write_calibrate_data_to_nv(PS_CALIDATA_NV_NUM, TOF_CALIDATA_NV_SIZE, "PSENSOR", (char *)temp))
		return -1;

	hwlog_info("write_ps_offset_to_nv temp: temp[0]=%d,temp[9]=%d,temp[19]=%d temp[27]=%d\n",
		temp[0], temp[9], temp[19], temp[TOF_CALIDATA_NV_SIZE - 1]);
	if (memcpy_s(data, TOF_CALIDATA_NV_SIZE,
		temp, TOF_CALIDATA_NV_SIZE) != EOK) {
		hwlog_err("%s:memcpy_s fail\n", __func__);
		return -1;
	}
	return 0;
}


static int ps_calibrate_save(const void *buf, int length)
{
	int temp_buf[3] = {0};
	int ret;

	if (!buf || length <= 0 || length > 12) { /* max 12 */
		hwlog_err("%s invalid argument", __func__);
		ps_calibration_res = EXEC_FAIL;
		return -1;
	}
	memcpy(temp_buf, buf, length);
	hwlog_info("%s:psensor calibrate ok, %d,%d,%d\n", __func__, temp_buf[0], temp_buf[1], temp_buf[2]);

	if (ps_external_ir_calibrate_flag == 1) {
		ps_external_ir_param.external_ir_pwindows_value = temp_buf[2] - temp_buf[1];
		ps_external_ir_param.external_ir_pwave_value = temp_buf[1] - temp_buf[0];
		ps_external_ir_param.external_ir_calibrate_noise = temp_buf[0];
		hwlog_info("%s:ps set offset,pwindows:%d,pwave:%d\n", __func__,
			ps_external_ir_param.external_ir_pwindows_value,
			ps_external_ir_param.external_ir_pwave_value);
	}

	ret = write_ps_offset_to_nv(temp_buf);
	if (ret) {
		hwlog_err("nv write fail.\n");
		ps_calibration_res = NV_FAIL;
		return -1;
	}
	ps_calibration_res = SUC;
	return 0;
}

static int tof_calibrate_save(const void *buf, int length)
{
	uint8_t temp_buf[TOF_CALIDATA_NV_SIZE] = {0};
	int ret;

	if (!buf || length <= 0 || length > TOF_CALIDATA_NV_SIZE) {
		hwlog_err("%s invalid argument", __func__);
		ps_calibration_res = EXEC_FAIL;
		return -1;
	}
	memcpy(temp_buf, buf, length);
	hwlog_info("%s:calibrate ok, buf[0]=%d, buf[9]=%d,buf[19]=%d, buf[27]=%d\n",
		__func__, temp_buf[0], temp_buf[9], temp_buf[19], temp_buf[TOF_CALIDATA_NV_SIZE - 1]);
	ret = write_tof_offset_to_nv(temp_buf);
	if (ret) {
		hwlog_err("nv write fail.\n");
		ps_calibration_res = NV_FAIL;
		return -1;
	}
	ps_calibration_res = SUC;
	return 0;
}

static ssize_t attr_ps_calibrate_write(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct ps_device_info *dev_info = NULL;
	unsigned long val = 0;
	struct read_info pkg_mcu;
	int ps_calibate_offset_0;
	int ps_calibate_offset_3;
	uint8_t calibrate_order;

	char content[CLI_CONTENT_LEN_MAX] = {0};
	char date_str[CLI_TIME_STR_LEN] = {0};
	const int32_t *ps_cali_data = NULL;
#ifdef SENSOR_DATA_ACQUISITION
	int32_t min_threshold_ps[PS_CAL_NUM] = { 0, 0, 0, -ps_data.offset_min };
	int32_t max_threshold_ps[PS_CAL_NUM] = {
		ps_data.ps_calib_20cm_threshold, ps_data.ps_calib_5cm_threshold,
		ps_data.ps_calib_3cm_threshold, ps_data.offset_max
	};
	struct sensor_eng_cal_test ps_test = { 0 };
	int i;
#endif

	dev_info = ps_get_device_info(TAG_PS);
	if (!dev_info)
		return -1;

	if ((get_sensor_tof_flag() != 1) &&
		(dev_info->chip_type == PS_CHIP_NONE) &&
		(ps_external_ir_calibrate_flag != 1)) {
		hwlog_info("ps sensor is not txc_ps_224 or ams_tmd2620 or avago_apds9110 or ams_tmd3725 or liteon_ltr582 or liteon_ltr2568,no need calibrate\n");
		return count;
	}

	if (strict_strtoul(buf, TO_DECIMALISM, &val))
		return -EINVAL;
	hwlog_info("ps or tof calibrate order is %d\n", val);
	if ((val < PS_XTALK_CALIBRATE) || (val > TOF_60CM_CALIBRATE)) {
		hwlog_err("set ps or tof calibrate val invalid,val=%lu\n", val);
		return count;
	}
	calibrate_order = (uint8_t)val;

	if ((val >= PS_XTALK_CALIBRATE && val <= PS_3CM_CALIBRATE) &&
		(get_sensor_tof_flag() != 1)) { /* ps calibrate */
		pkg_mcu = send_calibrate_cmd(TAG_PS, val, &ps_calibration_res);
		if (ps_calibration_res == COMMU_FAIL || ps_calibration_res == EXEC_FAIL) {
			goto save_log;
		} else if (pkg_mcu.errno == 0) {
			if (val == 1) {
				ps_calib_data[val - 1] = *((int32_t *)pkg_mcu.data);
				ps_calibate_offset_0 = (int16_t)(ps_calib_data[val - 1] & 0x0000ffff);
				ps_calibate_offset_3 = (int16_t)((ps_calib_data[val - 1] & 0xffff0000)>>16);
				ps_calib_data_for_data_collect[0] = (int32_t)ps_calibate_offset_0;
				ps_calib_data_for_data_collect[3] = (int32_t)ps_calibate_offset_3;
				hwlog_info("ps calibrate success, ps_calibate_offset_0=%d, ps_calibate_offset_3=%d\n",
					ps_calibate_offset_0, ps_calibate_offset_3);
				hwlog_info("ps calibrate success, data=%d, len=%d val=%d\n",
					ps_calib_data[val - 1], pkg_mcu.data_length, val);
			} else {
				ps_calib_data[val - 1] = *((int32_t *)pkg_mcu.data);
				ps_calib_data_for_data_collect[val - 1] = *((int32_t *)pkg_mcu.data);
				hwlog_info("ps calibrate success, data=%d, len=%d val=%d\n",
					ps_calib_data[val - 1], pkg_mcu.data_length, val);
			}
		}

		if (val == PS_5CM_CALIBRATE)
			ps_calibration_res = SUC;
		else
			ps_calibrate_save(ps_calib_data,
				3 * pkg_mcu.data_length); /* multiply 3 */
	} else { /* TOF calibrate */
		pkg_mcu = send_calibrate_cmd(TAG_TOF, val, &ps_calibration_res);
		if (ps_calibration_res == COMMU_FAIL || ps_calibration_res == EXEC_FAIL)
			goto save_log;
		else if (pkg_mcu.errno == 0)
			tof_calibrate_save(pkg_mcu.data, pkg_mcu.data_length);
	}

save_log:
	get_test_time(date_str, sizeof(date_str));
	ps_cali_data = (const int32_t *)ps_calib_data_for_data_collect;

#ifdef SENSOR_DATA_ACQUISITION
	ps_test.cal_value = (int32_t *)ps_cali_data;
	ps_test.first_item = PS_CALI_XTALK;
	ps_test.value_num = PS_CAL_NUM;
	ps_test.threshold_num = PS_THRESHOLD_NUM;
	ps_test.min_threshold = (int32_t *)min_threshold_ps;
	ps_test.max_threshold = (int32_t *)max_threshold_ps;
	memcpy(ps_test.name, PS_TEST_CAL, sizeof(PS_TEST_CAL));
	memcpy(ps_test.result, PS_CAL_RESULT, (strlen(PS_CAL_RESULT) + 1));
	for (i = 0; i < PS_CAL_NUM; i++)
		ps_test.test_name[i] = ps_test_name[i];
	enq_notify_work_sensor(ps_test);
#endif

	if (val >= PS_XTALK_CALIBRATE && val <= PS_3CM_CALIBRATE) {
		memset(&content, 0, sizeof(content));
		snprintf(content, CLI_CONTENT_LEN_MAX, PS_CALI_RAW_DATA, ps_calib_data[val - 1],
			((ps_calibration_res == SUC) ? "pass" : "fail"), (int)val,
			get_cali_error_code(ps_calibration_res), date_str);
		save_to_file(DATA_CLLCT, content);
	}
	return count;
}
static DEVICE_ATTR(ps_calibrate, 0664, attr_ps_calibrate_show, attr_ps_calibrate_write);

static int ps_external_ir_para_set(unsigned long mode)
{
	if ((mode < PS_MODE_MIN) || (mode > PS_MODE_MAX)) {
		hwlog_err("set ps switch mode val invalid,val=%lu\n", mode);
		return -EINVAL;
	}

	if (mode == MID_PS) {
		ps_extend_data.external_ir_mode_flag  = 1;
		ps_extend_data.min_proximity_value = ps_external_ir_param.external_ir_min_proximity_value;
		ps_extend_data.pwindows_value = ps_external_ir_param.external_ir_pwindows_value;
		ps_extend_data.pwave_value = ps_external_ir_param.external_ir_pwave_value;
		ps_extend_data.threshold_value  = ps_external_ir_param.external_ir_threshold_value;
		ps_extend_data.calibrate_noise  = ps_external_ir_param.external_ir_calibrate_noise;
	} else if (mode == NEAR_PS) {
		ps_extend_data.external_ir_mode_flag  = 1;
		ps_extend_data.min_proximity_value = ps_external_ir_param.external_ir_min_proximity_value;
		ps_extend_data.pwindows_value = ps_external_ir_param.external_ir_pwindows_value *
			ps_external_ir_param.external_ir_pwindows_ratio / PS_RATIO;
		ps_extend_data.pwave_value = ps_external_ir_param.external_ir_pwave_value *
			ps_external_ir_param.external_ir_pwave_ratio / PS_RATIO;
		ps_extend_data.threshold_value  = ps_external_ir_param.external_ir_threshold_value;
		ps_extend_data.calibrate_noise  = ps_external_ir_param.external_ir_calibrate_noise;
	} else {
		ps_extend_data.external_ir_mode_flag  = 0;
		ps_extend_data.min_proximity_value = ps_external_ir_param.internal_ir_min_proximity_value;
		ps_extend_data.pwindows_value = ps_external_ir_param.internal_ir_pwindows_value;
		ps_extend_data.pwave_value = ps_external_ir_param.internal_ir_pwave_value;
		ps_extend_data.threshold_value  = ps_external_ir_param.internal_ir_threshold_value;
	}

	hwlog_info("external_ir:%d,external_ir_gpio:%d\n",
		ps_extend_data.external_ir_mode_flag, ps_external_ir_param.external_ir_enable_gpio);
	hwlog_info("min_proximity:%d, pwindows:%d, pwave:%d, threshold_valule:%d",
		ps_extend_data.min_proximity_value, ps_extend_data.pwindows_value,
		ps_extend_data.pwave_value, ps_extend_data.threshold_value);
	return 0;
}

static void ps_external_ir_power_set(unsigned long mode)
{
	int ret;

	if (ps_external_ir_param.external_ir_powermode == 0) {
		if ((mode == MID_PS) || (mode == NEAR_PS))
			gpio_direction_output(ps_external_ir_param.external_ir_enable_gpio, 1);
		else
			gpio_direction_output(ps_external_ir_param.external_ir_enable_gpio, 0);

		return;
	}

	if ((mode == MID_PS) || (mode == NEAR_PS)) {
		if (!regulator_is_enabled(ps_external_ir_vdd)) {
			hwlog_info("ps switch enable vdd\n");
			ret = regulator_enable(ps_external_ir_vdd);
			if (ret < 0)
				hwlog_err("failed to enable regulator ps_external_ir_vdd\n");
		} else {
			hwlog_info("ps IR power has enable already, no need enable again\n");
		}
	} else {
		if (regulator_is_enabled(ps_external_ir_vdd)) {
			hwlog_info("ps switch disable vdd\n");
			ret = regulator_disable(ps_external_ir_vdd);
			if (ret < 0)
				hwlog_err("failed to disable regulator ps_external_ir_vdd\n");
		} else {
			hwlog_info("ps IR power has disable already, no need disable again\n");
		}
	}
}

static ssize_t attr_ps_switch_mode_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int val = ps_switch_mode;

	return snprintf(buf, PAGE_SIZE, "%d", val);
}

static ssize_t attr_ps_switch_mode_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned long val = 0;
	int ret;
	struct write_info pkg_ap = { 0 };
	struct read_info pkg_mcu = { 0 };
	pkt_parameter_req_t spkt;
	struct pkt_header *shd = (struct pkt_header *)&spkt;

	if (ps_external_ir_param.external_ir != 1) {
		hwlog_err(" external_ir not enable in this product. operation fail");
		return -EINVAL;
	}

	if (strict_strtoul(buf, TO_DECIMALISM, &val))
		return -EINVAL;

	ret = ps_external_ir_para_set(val);
	if (ret < 0)
		return count;

	pkg_ap.tag = TAG_PS;
	pkg_ap.cmd = CMD_CMN_CONFIG_REQ;
	spkt.subcmd = SUB_CMD_SET_RESET_PARAM_REQ;
	pkg_ap.wr_buf = &shd[1];
	pkg_ap.wr_len = sizeof(ps_extend_data) + SUBCMD_LEN;
	if (memcpy_s(spkt.para, sizeof(ps_extend_data),
		&ps_extend_data, sizeof(ps_extend_data)) != EOK) {
		hwlog_err("ps switch mode memcpy ps_extend_data error\n");
		return -EINVAL;
	}

	hwlog_err("ps switch mode val is %lu  len is %lu\n", val, sizeof(val));
	ret = write_customize_cmd(&pkg_ap,  &pkg_mcu, true);
	if (ret) {
		hwlog_err("send ps switch mode cmd to mcu fail,ret=%d\n", ret);
		return count;
	}

	if (pkg_mcu.errno != 0) {
		hwlog_err("ps switch mode fail, %d\n", pkg_mcu.errno);
	} else {
		ps_switch_mode = val;
		ps_external_ir_power_set(val);
		hwlog_info("ps switch mode  success, data len=%d\n", pkg_mcu.data_length);
	}

	return count;
}

static DEVICE_ATTR(ps_switch_mode, 0664, attr_ps_switch_mode_show,
	attr_ps_switch_mode_store);

static ssize_t attr_ps_support_mode_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int val = ps_support_mode;

	return snprintf(buf, PAGE_SIZE, "%d\n", val);
}

static DEVICE_ATTR(ps_support_mode, 0664, attr_ps_support_mode_show, NULL);

static ssize_t attr_als_calibrate_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int val = als_calibration_res;

	return snprintf(buf, PAGE_SIZE, "%d\n", val);
}

int write_als_offset_to_nv(char *temp)
{
	const uint16_t *poffset_data = (const uint16_t *)user_info.nv_data;
	struct als_device_info *dev_info = NULL;

	dev_info = als_get_device_info(TAG_ALS);
	if (!dev_info)
		return -1;

	if (!temp) {
		hwlog_err("%s fail, invalid para!\n", __func__);
		return -1;
	}

	if (write_calibrate_data_to_nv(ALS_CALIDATA_NV_NUM, ALS_CALIDATA_NV_SIZE, "LSENSOR", temp))
		return -1;
	if (memcpy_s((void *)dev_info->als_sensor_calibrate_data,
		MAX_SENSOR_CALIBRATE_DATA_LENGTH, (void *)temp,
		MAX_SENSOR_CALIBRATE_DATA_LENGTH) != EOK)
		return -1;
	hwlog_info("nve_direct_access als write temp %d %d %d %d %d %d\n",
		*poffset_data, *(poffset_data + 1), *(poffset_data + 2),
		*(poffset_data + 3), *(poffset_data + 4), *(poffset_data + 5));
	return 0;
}

#ifdef SENSOR_DATA_ACQUISITION
static void als_dark_noise_offset_enq_notify_work(const int item_id,
	uint16_t value, uint16_t min_threshold, uint16_t max_threshold)
{
	int ret;
	struct event offset_event;

	memset(&offset_event, 0, sizeof(offset_event));
	offset_event.item_id = item_id;
	memcpy(offset_event.device_name, ALS_TEST_CAL, sizeof(ALS_TEST_CAL));
	if (memcpy_s(offset_event.result, sizeof(offset_event.result),
		ALS_CAL_RESULT, (strlen(ALS_CAL_RESULT) + 1)) != EOK)
		return;
	memcpy(offset_event.test_name, ALS_DARK_CALI_NAME, sizeof(ALS_DARK_CALI_NAME));
	snprintf(offset_event.value, MAX_VAL_LEN, "%d", value);
	snprintf(offset_event.min_threshold, MAX_VAL_LEN, "%u", min_threshold);
	snprintf(offset_event.max_threshold, MAX_VAL_LEN, "%u", max_threshold);

	ret = enq_msg_data_in_sensorhub_single(offset_event);
	if (ret > 0)
		hwlog_info("%s succ!!item_id=%d\n", __func__, offset_event.item_id);
	else
		hwlog_info("%s failed!!\n", __func__);
}
#endif

static int als_calibrate_save(const void *buf, int length)
{
	const uint16_t *poffset_data = (const uint16_t *)buf;
	int ret;

	if (!buf || length <= 0) {
		hwlog_err("%s invalid argument", __func__);
		als_calibration_res = EXEC_FAIL;
		return -1;
	}

	hwlog_info("%s:als calibrate ok, %d  %d  %d %d  %d  %d\n", __func__,
		*poffset_data, *(poffset_data + 1), *(poffset_data + 2),
		*(poffset_data + 3), *(poffset_data + 4), *(poffset_data + 5));
	ret = write_als_offset_to_nv((char *)buf);
	if (ret) {
		hwlog_err("nv write fail.\n");
		als_calibration_res = NV_FAIL;
		return -1;
	}
	als_calibration_res = SUC;
	return 0;
}

static const char *als_calibrate_param[] = {
	ALS_CALI_R, ALS_CALI_G, ALS_CALI_B, ALS_CALI_C, ALS_CALI_LUX, ALS_CALI_CCT,
};

#define ALS_DARK_NOISE_OFFSET_MAX 10
#define ALS_DARK_NOISE_OFFSET_MIN 0

static int als_calibrate_on_enter(int32_t tag,
	struct als_device_info *dev_info)
{
	int ret;
	interval_param_t param = { 0 };

	if (sensor_status.opened[TAG_ALS] == 0) { /* if ALS is not opened, open first */
		als_close_after_calibrate = true;
		hwlog_info("send als open cmd(during calibrate) to mcu.\n");
		ret = inputhub_sensor_enable(TAG_ALS, true);
		if (ret) {
			als_calibration_res = COMMU_FAIL;
			hwlog_err("send ALS open cmd(during calibrate) to mcu fail,ret=%d\n", ret);
			return -1;
		}
	} else {
		als_close_after_calibrate = false;
	}

	if ((sensor_status.delay[TAG_ALS] == 0) ||
		(sensor_status.delay[TAG_ALS] > ALS_PERIOD_MAX)) {
		hwlog_info("send als setdelay cmd(during calibrate) to mcu.\n");
		param.period = ALS_PERIOD_MAX;
		ret = inputhub_sensor_setdelay(TAG_ALS, &param);
		if (ret) {
			als_calibration_res = COMMU_FAIL;
			hwlog_err("send ALS set delay cmd(during calibrate) to mcu fail,ret=%d\n", ret);
			return -1;
		}
	}

	return 0;
}

static void als_calibrate_on_exit(int32_t tag,
	struct als_device_info *dev_info)
{
	int ret;

	if (als_close_after_calibrate == true) {
		als_close_after_calibrate = false;
		hwlog_info("send als close cmd(during calibrate) to mcu.\n");
		ret = inputhub_sensor_enable(TAG_ALS, false);
		if (ret) {
			als_calibration_res = COMMU_FAIL;
			hwlog_err("send ALS close cmd(during calibrate) to mcu fail,ret=%d\n", ret);
			return;
		}
	}
}

static ssize_t attr_als_calibrate_write(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct als_device_info *dev_info = NULL;
	uint32_t t;
	unsigned long val = 0;
	int ret, i;
	struct read_info pkg_mcu;
	char content[CLI_CONTENT_LEN_MAX] = { 0 };
	const int32_t *als_cali_data = NULL;
	char date_str[CLI_TIME_STR_LEN] = { 0 };
#ifdef SENSOR_DATA_ACQUISITION
	const uint16_t *cali_data_u16 = NULL;
	int32_t min_threshold_als[ALS_CAL_NUM];
	int32_t max_threshold_als[ALS_CAL_NUM];
	struct sensor_eng_cal_test als_test = { 0 };
#endif

	dev_info = als_get_device_info(TAG_ALS);
	if (!dev_info) {
		hwlog_err("%s: dev_info is NULL\n", __func__);
		return -1;
	}

	t = dev_info->chip_type;
	if (t != ALS_CHIP_ROHM_RGB && t != ALS_CHIP_AVAGO_RGB && t != ALS_CHIP_AMS_TMD3725_RGB &&
		t != ALS_CHIP_LITEON_LTR582 && dev_info->is_cali_supported != 1 &&
		t != ALS_CHIP_APDS9999_RGB && t != ALS_CHIP_AMS_TMD3702_RGB &&
		t != ALS_CHIP_APDS9253_RGB && t != ALS_CHIP_VISHAY_VCNL36658 &&
		t != ALS_CHIP_BU27006 && t != ALS_CHIP_TCS3707 && t != ALS_CHIP_SY3133 && t != ALS_CHIP_HX32062SE && t != ALS_CHIP_SIP3510 &&
		t != ALS_CHIP_TMD2755 && t != ALS_CHIP_STK33562 && t != ALS_CHIP_MN78911) {
		hwlog_info("als sensor is not rohm_bh1745 or avago apds9251 or ams_tmd3725 or liteon_ltr582 , is_cali_supported = %d, no need calibrate\n",
			dev_info->is_cali_supported);
		return count;
	}

	ret = get_tpcolor_from_nv();
	if (ret)
		hwlog_err("cali write, get_tpcolor_from_nv fail, ret=%d\n", ret);
	else
		hwlog_info("cali write, get_tpcolor_from_nv success\n");

	if (strict_strtoul(buf, TO_DECIMALISM, &val))
		return -EINVAL;

	if (val != ALS_CALI_ENV_LIGHT && val != ALS_CALI_ENV_DARK)
		return count;

	ret = als_calibrate_on_enter(TAG_ALS, dev_info);
	if (ret)
		return count;

	msleep(350); /* sleep 350 ms */
	/* send calibrate command, need set delay first */

	pkg_mcu = send_calibrate_cmd(TAG_ALS, val, &als_calibration_res);
	if (als_calibration_res == COMMU_FAIL)
		return count;
	else if (pkg_mcu.errno == 0)
		als_calibrate_save(pkg_mcu.data, pkg_mcu.data_length);

	get_test_time(date_str, sizeof(date_str));
	als_cali_data = (const int32_t *)pkg_mcu.data;

	if (val == ALS_CALI_ENV_DARK) {
#ifdef SENSOR_DATA_ACQUISITION
		cali_data_u16 = (const uint16_t *)pkg_mcu.data;
		als_dark_noise_offset_enq_notify_work(ALS_CALI_DARK_OFFSET_MSG, *cali_data_u16,
				ALS_DARK_NOISE_OFFSET_MIN, ALS_DARK_NOISE_OFFSET_MAX);
#endif
	} else {
#ifdef SENSOR_DATA_ACQUISITION
		cali_data_u16 = (const uint16_t *)pkg_mcu.data;
		int32_t als_cali_data_int32[ALS_CAL_NUM];

		for (i = 0; i < ALS_CAL_NUM; i++) {
			als_cali_data_int32[i] = *(cali_data_u16 + i);
			min_threshold_als[i] = dev_info->min_thres;
			max_threshold_als[i] = dev_info->max_thres;
		}

		hwlog_info("als calibrate data for collect, %d %d %d %d %d %d\n",
			*cali_data_u16, *(cali_data_u16 + 1), *(cali_data_u16 + 2),
			*(cali_data_u16 + 3), *(cali_data_u16 + 4), *(cali_data_u16 + 5));

		als_test.cal_value = (int32_t *)als_cali_data_int32;
		als_test.first_item = ALS_CALI_R_MSG;
		als_test.value_num = ALS_CAL_NUM;
		als_test.threshold_num = ALS_THRESHOLD_NUM;
		als_test.min_threshold = (int32_t *)min_threshold_als;
		als_test.max_threshold = (int32_t *)max_threshold_als;

		memcpy(als_test.name, ALS_TEST_CAL, sizeof(ALS_TEST_CAL));
		memcpy(als_test.result, ALS_CAL_RESULT, (strlen(ALS_CAL_RESULT) + 1));
		for (i = 0; i < ALS_CAL_NUM; i++)
			als_test.test_name[i] = als_test_name[i];
		enq_notify_work_sensor(als_test);
#endif

		for (i = 0; i < ARRAY_SIZE(als_calibrate_param); i++) {
			memset(&content, 0, sizeof(content));
			snprintf(content, CLI_CONTENT_LEN_MAX, als_calibrate_param[i], *(als_cali_data),
				((als_calibration_res == SUC) ? "pass" : "fail"),
				get_cali_error_code(als_calibration_res), date_str);
			save_to_file(DATA_CLLCT, content);
		}
	}

	als_calibrate_on_exit(TAG_ALS, dev_info);
	return count;
}

static DEVICE_ATTR(als_calibrate, 0664, attr_als_calibrate_show, attr_als_calibrate_write);

static ssize_t attr_cap_prox_calibrate_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	u32 *pcaldata = NULL;

	if (read_calibrate_data_from_nv(CAP_PROX_CALIDATA_NV_NUM, CAP_PROX_CALIDATA_NV_SIZE, "Csensor")) {
		hwlog_err("nve_direct_access read error\n");
		return snprintf(buf, PAGE_SIZE, "%d\n", return_cap_prox_calibration);
	}

	pcaldata = (u32 *)(user_info.nv_data + 4); /* 4 bytes offset */
	return snprintf(buf, PAGE_SIZE, "%d:%08x %08x %08x\n",
		return_cap_prox_calibration, pcaldata[0], pcaldata[1], pcaldata[2]);
}

static int cap_prox_calibrate_save(void *buf, int length)
{
	u32 *pcaldata = NULL;

	if (!buf) {
		hwlog_err("%s invalid argument", __func__);
		return_cap_prox_calibration = EXEC_FAIL;
		return -1;
	}
	pcaldata = (u32 *)buf;
	hwlog_err("%s:cap_prox calibrate ok, %08x  %08x  %08x\n", __func__,
		pcaldata[0], pcaldata[1], pcaldata[2]);

	if (read_calibrate_data_from_nv(CAP_PROX_CALIDATA_NV_NUM, CAP_PROX_CALIDATA_NV_SIZE, "Csensor")) {
		return_cap_prox_calibration = EXEC_FAIL;
		hwlog_err("nve_direct_access read error\n");
		return -2; /* -2: read error */
	}

	if (write_calibrate_data_to_nv(CAP_PROX_CALIDATA_NV_NUM, length, "Csensor", buf)) {
		return_cap_prox_calibration = EXEC_FAIL;
		hwlog_err("nve_direct_access write error\n");
		return -3; /* -3: write error */
	}
	return_cap_prox_calibration = SUC;
	return 0;
}

static void cap_prox_calibrate_work_func(struct work_struct *work)
{
	int ret;

	hwlog_info("cap_prox calibrate work enter ++\n");
	ret = cap_prox_calibrate_save(cap_prox_calibrate_data, cap_prox_calibrate_len);
	if (ret < 0)
		hwlog_err("nv write faild.\n");
	hwlog_info("cap_prox calibrate work enter --\n");
}

static void attr_abov_calibrate_write(int calibrate_index, uint16_t *abov_data)
{
	switch (calibrate_index) {
	case 1: /* 1: near data */
		sar_calibrate_datas.abov_cali_data.diff[0] = abov_data[0];
		sar_calibrate_datas.abov_cali_data.diff[1] = abov_data[1];
		break;
	case 2: /* 2: far data */
		sar_calibrate_datas.abov_cali_data.offset[0] = abov_data[0];
		sar_calibrate_datas.abov_cali_data.offset[1] = abov_data[1];
		break;
	default:
		hwlog_err("abov a96t3x6 sar calibrate err\n");
		break;
	}
	hwlog_info("abov a96t3x6 calirate write data %u %u\n", abov_data[0], abov_data[1]);
	hwlog_info("abov_data offset[1]:%d offset[2]:%d diff[1]:%d diff[2]:%d\n",
		sar_calibrate_datas.abov_cali_data.offset[0],
		sar_calibrate_datas.abov_cali_data.offset[1],
		sar_calibrate_datas.abov_cali_data.diff[0],
		sar_calibrate_datas.abov_cali_data.diff[1]);
	cap_prox_calibrate_len = sizeof(sar_calibrate_datas);
	if (cap_prox_calibrate_len > sizeof(cap_prox_calibrate_data))
		cap_prox_calibrate_len = sizeof(cap_prox_calibrate_data);

	memcpy(cap_prox_calibrate_data, &sar_calibrate_datas,
		cap_prox_calibrate_len);
}

static int cap_prox_calibrate_psoc4000(int calibrate_index, struct read_info *pkg_mcu)
{
	uint16_t cypress_data[2] = {0}; /* first 2 u16 from pkg_mcu->data */

	if (memcpy_s(cypress_data, sizeof(cypress_data),
		pkg_mcu->data, sizeof(cypress_data)) != EOK)
		return -1;
	switch (calibrate_index) {
	case 0: /* 0: free data */
		sar_calibrate_datas.cypres_cali_data.sar_idac = cypress_data[0];
		sar_calibrate_datas.cypres_cali_data.raw_data = cypress_data[1];
		break;
	case 1: /* 1: near data */
		sar_calibrate_datas.cypres_cali_data.near_signaldata = cypress_data[0];
		break;
	case 2: /* 2: far data */
		sar_calibrate_datas.cypres_cali_data.far_signaldata = cypress_data[0];
		break;
	default:
		hwlog_err("sar calibrate err\n");
		break;
	}
	hwlog_info("cypress_data %u %u\n", cypress_data[0], cypress_data[1]);
	hwlog_info("idac:%d,rawdata:%d,near:%d,far:%d\n",
		sar_calibrate_datas.cypres_cali_data.sar_idac,
		sar_calibrate_datas.cypres_cali_data.raw_data,
		sar_calibrate_datas.cypres_cali_data.near_signaldata,
		sar_calibrate_datas.cypres_cali_data.far_signaldata);
	cap_prox_calibrate_len = sizeof(cap_prox_calibrate_data);
	if (memcpy_s(cap_prox_calibrate_data,
		cap_prox_calibrate_len, &sar_calibrate_datas,
		cap_prox_calibrate_len) != EOK)
		return -1;

	return 0;
}

static int cap_prox_calibrate_sx9323(int calibrate_index, struct read_info *pkg_mcu)
{
	uint16_t semtech = 0;

	if (memcpy_s(&semtech, sizeof(semtech), pkg_mcu->data,
		sizeof(semtech)) != EOK)
		return -1;
	switch (calibrate_index) {
	case 1: /* near data */
		sar_calibrate_datas.semtech_cali_data.diff = semtech;
		break;
	case 2: /* far data */
		sar_calibrate_datas.semtech_cali_data.offset = semtech;
		break;
	default:
		hwlog_err(" semtech sar calibrate err\n");
		break;
	}
	hwlog_info("semtech_data %u\n", semtech);
	hwlog_info("semtech_data offset:%d,diff:%d\n",
		sar_calibrate_datas.semtech_cali_data.offset,
		sar_calibrate_datas.semtech_cali_data.diff);
	cap_prox_calibrate_len = pkg_mcu->data_length;
	if (cap_prox_calibrate_len > sizeof(cap_prox_calibrate_data))
		cap_prox_calibrate_len = sizeof(cap_prox_calibrate_data);
	if (memcpy_s(cap_prox_calibrate_data,
		cap_prox_calibrate_len, &sar_calibrate_datas,
		cap_prox_calibrate_len) != EOK)
		return -1;

	return 0;
}

static int cap_prox_calibrate_aw9610x(int calibrate_index, struct read_info *pkg_mcu)
{
	uint16_t aw9610data = 0;

	if (memcpy_s(&aw9610data, sizeof(aw9610data), pkg_mcu->data,
		sizeof(aw9610data)) != EOK)
		return -1;
	switch (calibrate_index) {
	case 1: /* near data */
		sar_calibrate_datas.aw9610_cali_data.diff = aw9610data;
		break;
	case 2: /* far data */
		sar_calibrate_datas.aw9610_cali_data.offset = aw9610data;
		break;
	default:
		hwlog_err("aw9610x sar calibrate err\n");
		break;
	}
	hwlog_info("aw9610data %u\n", aw9610data);
	hwlog_info("aw9610data offset:%d,diff:%d\n",
		sar_calibrate_datas.aw9610_cali_data.offset,
		sar_calibrate_datas.aw9610_cali_data.diff);
	cap_prox_calibrate_len = pkg_mcu->data_length;
	if (cap_prox_calibrate_len > sizeof(cap_prox_calibrate_data))
		cap_prox_calibrate_len = sizeof(cap_prox_calibrate_data);
	if (memcpy_s(cap_prox_calibrate_data,
		cap_prox_calibrate_len, &sar_calibrate_datas,
		cap_prox_calibrate_len) != EOK)
		return -1;

	return 0;
}

static ssize_t attr_cap_prox_calibrate_write(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned long val = 0;
	int calibrate_index;
	struct read_info pkg_mcu;
	uint16_t abov_data[CAP_PROX_CAL_NUM] = { 0 };
	char *chip_info = get_sensor_chip_info_address(CAP_PROX);

	hwlog_info("%s\n", __func__);
	if (strict_strtoul(buf, TO_DECIMALISM, &val))
		return -EINVAL;

	if (strlen(chip_info) == 0) {
		hwlog_err("no sar sensor\n");
		return -EINVAL;
	}
	/* init the result failed */
	return_cap_prox_calibration = EXEC_FAIL;
	calibrate_index = (int)val;

	hwlog_info("cap_prox_calibrate : %lu\n", val);
	if (val < 0)
		return count;
	pkg_mcu = send_calibrate_cmd(TAG_CAP_PROX, val, &return_cap_prox_calibration);
	if (return_cap_prox_calibration == COMMU_FAIL ||
		pkg_mcu.errno != 0)
		return count;

	if (!strncmp(chip_info, "huawei,cypress_sar_psoc4000",
		strlen("huawei,cypress_sar_psoc4000"))) {
		if (cap_prox_calibrate_psoc4000(calibrate_index, &pkg_mcu))
			return -1;
	} else if (!strncmp(chip_info, "huawei,semtech-sx9323",
		strlen("huawei,semtech-sx9323"))) {
		if (cap_prox_calibrate_sx9323(calibrate_index, &pkg_mcu))
			return -1;
	} else if (!strncmp(chip_info, "huawei,abov-a96t3x6",
		strlen("huawei,abov-a96t3x6"))) {
		if (memcpy_s(&abov_data, sizeof(abov_data), pkg_mcu.data,
			sizeof(abov_data)) != EOK)
			return -1;
		attr_abov_calibrate_write(calibrate_index, abov_data);
	} else if (!strncmp(chip_info, "huawei,awi-aw9610x",
		strlen("huawei,awi-aw9610x"))) {
		if (cap_prox_calibrate_aw9610x(calibrate_index, &pkg_mcu))
			return -1;
	} else {
		cap_prox_calibrate_len = pkg_mcu.data_length;
		if (cap_prox_calibrate_len > sizeof(cap_prox_calibrate_data))
			cap_prox_calibrate_len = sizeof(cap_prox_calibrate_data);
		if (memcpy_s(cap_prox_calibrate_data,
			sizeof(cap_prox_calibrate_data), pkg_mcu.data,
			sizeof(cap_prox_calibrate_data)) != EOK)
			return -1;
	}
	INIT_WORK(&cap_prox_calibrate_work, cap_prox_calibrate_work_func);
	queue_work(system_power_efficient_wq, &cap_prox_calibrate_work);
#ifdef SENSOR_DATA_ACQUISITION
	cap_prox_do_enq_work(calibrate_index);
#endif
	return count;
}

static DEVICE_ATTR(cap_prox_calibrate, 0664, attr_cap_prox_calibrate_show,
	attr_cap_prox_calibrate_write);

static ssize_t attr_cap_prox_freespace_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	u16 *pfreespace = NULL;

	if (read_calibrate_data_from_nv(CAP_PROX_CALIDATA_NV_NUM, CAP_PROX_CALIDATA_NV_SIZE, "Csensor")) {
		hwlog_err("nve_direct_access read error\n");
		return -1;
	}

	pfreespace = (u16 *)user_info.nv_data;
	return snprintf(buf, MAX_STR_SIZE, "%04x%04x\n", pfreespace[0], pfreespace[1]);
}

static ssize_t attr_cap_prox_freespace_write(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	uint32_t val;
	char *pt = NULL;
	u16 *pfreespace = NULL;

	val = simple_strtoul(buf, &pt, 0);

	if (read_calibrate_data_from_nv(CAP_PROX_CALIDATA_NV_NUM,
		CAP_PROX_CALIDATA_NV_SIZE, "Csensor")) {
		hwlog_err("nve_direct_access read error\n");
		return -1; /* -1: read error */
	}

	pfreespace = (u16 *)user_info.nv_data;
	pfreespace[0] = (u16)((val >> 16) & 0xffff);
	pfreespace[1] = (u16)(val & 0xffff);

	if (write_calibrate_data_to_nv(CAP_PROX_CALIDATA_NV_NUM,
		CAP_PROX_CALIDATA_NV_SIZE, "Csensor", (char *)pfreespace)) {
		hwlog_err("nve_direct_access write error\n");
		return -2; /* -2: write error */
	}

	return count;
}


static DEVICE_ATTR(cap_prox_freespace, 0664, attr_cap_prox_freespace_show,
	attr_cap_prox_freespace_write);

/* if val is odd, then last status is sleep, if is even number, then last status is wakeup */
static ssize_t attr_iom3_sr_test_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned long val = 0;
	unsigned long times;

	if (strict_strtoul(buf, TO_DECIMALISM, &val))
		return -EINVAL;

	times = val;

	if (val > 0) {
		for (; val > 0; val--) {
			disable_sensors_when_suspend();
			tell_ap_status_to_mcu(ST_SLEEP);
			msleep(2); /* sleep 2 ms */
			tell_ap_status_to_mcu(ST_WAKEUP);
			enable_sensors_when_resume();
		}

		if (times % 2) { /* 2: odd or even */
			tell_ap_status_to_mcu(ST_SLEEP);
			enable_sensors_when_resume();
		}
	}
	return count;
}

static DEVICE_ATTR(iom3_sr_test, 0660, NULL, attr_iom3_sr_test_store);

int fingersense_commu(unsigned int cmd, unsigned int pare,
	unsigned int responsed, bool is_subcmd)
{
	int ret;
	struct write_info pkg_ap = { 0 };
	struct read_info pkg_mcu = { 0 };
	pkt_parameter_req_t cpkt;
	struct pkt_header *hd = (struct pkt_header *)&cpkt;

	if (is_subcmd) {
		pkg_ap.tag = TAG_FINGERSENSE;
		pkg_ap.cmd = CMD_CMN_CONFIG_REQ;
		cpkt.subcmd = cmd;
		pkg_ap.wr_buf = &hd[1];
		pkg_ap.wr_len = sizeof(pare) + SUBCMD_LEN;
		memcpy(cpkt.para, &pare, sizeof(pare));
	} else {
		pkg_ap.tag = TAG_FINGERSENSE;
		pkg_ap.cmd = cmd;
		pkg_ap.wr_buf = &pare;
		pkg_ap.wr_len = sizeof(pare);
	}

	if (responsed == NO_RESP)
		ret = write_customize_cmd(&pkg_ap, NULL, true); /* enable/disable fingersense is no response */
	else
		ret = write_customize_cmd(&pkg_ap, &pkg_mcu, true); /* request fingersense data need response */
	if (ret) {
		hwlog_err("send finger sensor cmd %d to mcu fail,ret=%d\n", cmd, ret);
		return ret;
	}
	if (pkg_mcu.errno != 0)
		hwlog_err("send finger sensor cmd %d to mcu fail %d\n", cmd, pkg_mcu.errno);
	return ret;
}

int fingersense_enable(unsigned int enable)
{
	unsigned int cmd;
	unsigned int delay = 10000;
	unsigned int ret;

	if (enable == 1) {
		cmd = CMD_CMN_OPEN_REQ;
		ret = fingersense_commu(cmd, enable, NO_RESP, false);
		if (ret) {
			hwlog_err("%s: finger sense enable fail\n", __func__);
			return ret;
		}

		cmd = CMD_CMN_INTERVAL_REQ;
		ret = fingersense_commu(cmd, delay, NO_RESP, false);
		if (ret) {
			hwlog_err("%s: set delay fail\n", __func__);
			return ret;
		}
		hwlog_info("%s: finger sense enable succsess\n", __func__);
	} else {
		cmd = CMD_CMN_CLOSE_REQ;
		ret = fingersense_commu(cmd, enable, NO_RESP, false);
		if (ret) {
			hwlog_info("%s: finger sense close fail\n", __func__);
			return ret;
		}
		hwlog_info("%s: finger sense close succsess\n", __func__);
	}

	return 0;
}

static ssize_t attr_set_fingersense_enable(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	unsigned long val = 0;
	int ret;

	if (strict_strtoul(buf, TO_DECIMALISM, &val)) {
		hwlog_err("%s: finger sense enable val(%lu) invalid", __func__, val);
		return -EINVAL;
	}

	hwlog_info("%s: finger sense enable val (%ld)\n", __func__, val);
	if ((val != 0) && (val != 1)) {
		hwlog_err("%s:finger sense set enable fail, invalid val\n", __func__);
		return size;
	}

	if (fingersense_enabled == val) {
		hwlog_info("%s:finger sense already at seted state,fingersense_enabled:%d\n",
			__func__, fingersense_enabled);
		return size;
	}
	ret = fingersense_enable(val);
	if (ret) {
		hwlog_err("%s: finger sense enable fail: %d\n", __func__, ret);
		return size;
	}
	fingersense_enabled = val;

	return size;
}

static ssize_t attr_get_fingersense_enable(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return snprintf(buf, MAX_STR_SIZE, "%d\n", fingersense_enabled);
}

static DEVICE_ATTR(set_fingersense_enable, 0660, attr_get_fingersense_enable,
	attr_set_fingersense_enable);

static ssize_t attr_fingersense_data_ready(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	bool *ready = get_fingersense_data_ready();

	return snprintf_s(buf, MAX_STR_SIZE, MAX_STR_SIZE - 1,
		"%d\n", *ready);
}

static DEVICE_ATTR(fingersense_data_ready, 0440, attr_fingersense_data_ready, NULL);

static ssize_t attr_fingersense_latch_data(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int size;
	bool *ready = get_fingersense_data_ready();
	s16 *data = get_fingersense_data();

	size = ((sizeof(*data) * FINGERSENSE_DATA_NSAMPLES) < MAX_STR_SIZE) ?
		(sizeof(*data) * FINGERSENSE_DATA_NSAMPLES) : MAX_STR_SIZE;
	if ((!(*ready)) || (!fingersense_enabled)) {
		hwlog_err("%s:fingersense zaxix not ready %d or not enable %d\n",
			__func__, *ready, fingersense_enabled);
		return size;
	}
	if (memcpy_s(buf, MAX_STR_SIZE, (char *)data, size) != EOK) {
		hwlog_err("%s memcpy_s error\n", __func__);
		return -1;
	}

	return size;
}

static DEVICE_ATTR(fingersense_latch_data, 0440, attr_fingersense_latch_data, NULL);

/* Calculate whether a is in the range of [b, c] */
int is_time_inrange(unsigned long a, unsigned long b, unsigned long c)
{
	return ((long)(a - b) >= 0) && ((long)(a - c) <= 0);
}

static ssize_t attr_fingersense_req_data(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int ret;
	unsigned int skip = 0;
	unsigned long local_jiffies = jiffies;
	unsigned long flags = 0;
	bool *ready = get_fingersense_data_ready();
	bool *intrans = get_fingersense_data_intrans();
	spinlock_t *lock = get_fsdata_lock();

#if defined(CONFIG_HISI_VIBRATOR)
	if (get_hall_number() == HOLSTER_DOUBLE_HALL_MODE) {
		if ((vibrator_shake == 1) || ((hall_value & DOUBLE_HALL_BIT_0)
			&& (hall_value & DOUBLE_HALL_BIT_3))) {
			hwlog_err("coverd, vibrator shaking, not send fingersense req data cmd to mcu\n");
			return -1;
		}
	} else if ((vibrator_shake == 1) || (HALL_COVERD & hall_value)) {
		hwlog_err("coverd, vibrator shaking, not send fingersense req data cmd to mcu\n");
		return -1;
	}
#endif
	if (!fingersense_enabled) {
		hwlog_err("%s: finger sense not enable,  dont req data\n", __func__);
		return -1;
	}
	spin_lock_irqsave(lock, flags);

	/* We started transmitting the data in recent time. It's just on the way. Wait for it. */
	if ((*intrans) && is_time_inrange(fingersense_data_ts,
		local_jiffies - FINGERSENSE_TRANS_TOUT, local_jiffies))
		skip = 1;

	/* The data was collected a short while ago. Just use it. */
	if ((*ready) && (is_time_inrange(fingersense_data_ts,
		local_jiffies - FINGERSENSE_FRESH_TIME, local_jiffies)))
		skip = 1;

	if (skip) {
		spin_unlock_irqrestore(lock, flags);
		return size;
	}
	*ready = false;
	*intrans = true;   /* the data is on the way */
	fingersense_data_ts = jiffies;     /* record timestamp for the data */
	spin_unlock_irqrestore(lock, flags);
	ret = fingersense_commu(SUB_CMD_ACCEL_FINGERSENSE_REQ_DATA_REQ,
		SUB_CMD_ACCEL_FINGERSENSE_REQ_DATA_REQ, NO_RESP, true);
	if (ret) {
		spin_lock_irqsave(lock, flags);
		*intrans = false;
		spin_unlock_irqrestore(lock, flags);
		hwlog_err("%s: finger sense send requst data failed\n", __func__);
	}
	return size;
}

void preread_fingersense_data(void)
{
#if defined(CONFIG_HISI_VIBRATOR)
	if ((vibrator_shake == 1) || (HALL_COVERD & (unsigned int)hall_value))
		return;
#endif

	if (!fingersense_enabled)
		return;

	attr_fingersense_req_data(NULL, NULL, NULL, (unsigned long)0);
}
EXPORT_SYMBOL(preread_fingersense_data);

static DEVICE_ATTR(fingersense_req_data, 0220, NULL, attr_fingersense_req_data);

/* acc enable node */
#define SHOW_ENABLE_FUNC(NAME, TAG) \
static ssize_t show_##NAME##_enable_result(struct device *dev,	struct device_attribute *attr, char *buf) \
{ \
	return snprintf(buf, MAX_STR_SIZE, "%d\n", sensor_status.status[TAG]); \
}

#define STORE_ENABLE_FUNC(NAME, TAG, CMD1, CMD2) \
static ssize_t attr_set_##NAME##_enable(struct device *dev, struct device_attribute *attr, const char *buf, size_t size) \
{ \
	unsigned long val = 0; \
	int ret; \
	struct write_info pkg_ap; \
	struct read_info pkg_mcu;  \
	memset(&pkg_ap, 0, sizeof(pkg_ap)); \
	memset(&pkg_mcu, 0, sizeof(pkg_mcu)); \
	if (strict_strtoul(buf, 10, &val)) \
		return -EINVAL; \
	if (val == 1) { \
		pkg_ap.tag = TAG; \
		pkg_ap.cmd = CMD1; \
		pkg_ap.wr_buf = NULL; \
		pkg_ap.wr_len = 0; \
		ret = write_customize_cmd(&pkg_ap,  &pkg_mcu, true); \
		if (ret) { \
			hwlog_err("send %s enable cmd to mcu fail,ret=%d\n", #NAME, ret); \
			return size; \
		} \
		if (pkg_mcu.errno != 0) \
			hwlog_err("set %s enable fail\n", #NAME); \
		else \
			hwlog_info("%s enable success\n", #NAME); \
		if (((TAG) == TAG_ACCEL) && (acc_close_after_calibrate == true)) { \
			acc_close_after_calibrate = false; \
			hwlog_info("%s received open command during calibrate, will not close after calibrate!\n", #NAME); \
		} \
	} else { \
		pkg_ap.tag = TAG; \
		pkg_ap.cmd = CMD2; \
		pkg_ap.wr_buf = NULL; \
		pkg_ap.wr_len = 0; \
		ret = write_customize_cmd(&pkg_ap,  &pkg_mcu, true); \
		if (ret) { \
			hwlog_err("send %s disable cmd to mcu fail,ret=%d\n", #NAME, ret); \
			return size; \
		} \
		if (pkg_mcu.errno != 0) \
			hwlog_err("set %s disable fail\n", #NAME); \
		else \
			hwlog_info("%s disable success\n", #NAME); \
	} \
	return size; \
}

#define SHOW_DELAY_FUNC(NAME, TAG) \
static ssize_t show_##NAME##_delay_result(struct device *dev, struct device_attribute *attr, char *buf) \
{ \
	return snprintf(buf, MAX_STR_SIZE, "%d\n", sensor_status.delay[TAG]); \
}

#define STORE_DELAY_FUNC(NAME, TAG, CMD) \
static ssize_t attr_set_##NAME##_delay(struct device *dev, struct device_attribute *attr, const char *buf, size_t size) \
{ \
	unsigned long val = 0; \
	int ret; \
	struct write_info pkg_ap; \
	struct read_info pkg_mcu; \
	pkt_cmn_interval_req_t cpkt; \
	struct pkt_header *hd = (struct pkt_header *)&cpkt; \
\
	memset(&pkg_ap, 0, sizeof(pkg_ap)); \
	memset(&pkg_mcu, 0, sizeof(pkg_mcu)); \
	memset(&cpkt, 0, sizeof(cpkt)); \
	if (sensor_status.opened[TAG] == 0) { \
		hwlog_err("send tag %d delay must be opend first!\n", TAG); \
		return -EINVAL; \
	} \
	if (strict_strtoul(buf, 10, &val)) \
		return -EINVAL; \
	if (val >= 10 && val < 1000) { \
		pkg_ap.tag = TAG; \
		pkg_ap.cmd = CMD_CMN_INTERVAL_REQ; \
		cpkt.param.period = val; \
		pkg_ap.wr_buf = &hd[1]; \
		pkg_ap.wr_len = sizeof(cpkt.param); \
		ret = write_customize_cmd(&pkg_ap,  &pkg_mcu, true); \
		if (ret) { \
			hwlog_err("send %s delay cmd to mcu fail,ret=%d\n", #NAME, ret); \
			return size; \
		} \
		if (pkg_mcu.errno != 0) \
			hwlog_err("set %s delay fail\n", #NAME); \
		else \
			hwlog_info("set %s delay (%ld)success\n", #NAME, val); \
	} \
	return size; \
}

SHOW_ENABLE_FUNC(acc, TAG_ACCEL)
STORE_ENABLE_FUNC(acc, TAG_ACCEL, CMD_CMN_OPEN_REQ, CMD_CMN_CLOSE_REQ)
static DEVICE_ATTR(acc_enable, 0664, show_acc_enable_result, attr_set_acc_enable);
SHOW_DELAY_FUNC(acc, TAG_ACCEL)
STORE_DELAY_FUNC(acc, TAG_ACCEL, CMD_CMN_INTERVAL_REQ)
static DEVICE_ATTR(acc_setdelay, 0664, show_acc_delay_result, attr_set_acc_delay);

static DEVICE_ATTR(gsensor_gather_enable, 0664, NULL, attr_set_gsensor_gather_enable);

SHOW_ENABLE_FUNC(gyro, TAG_GYRO)
STORE_ENABLE_FUNC(gyro, TAG_GYRO, CMD_CMN_OPEN_REQ, CMD_CMN_CLOSE_REQ)
static DEVICE_ATTR(gyro_enable, 0664, show_gyro_enable_result, attr_set_gyro_enable);
SHOW_DELAY_FUNC(gyro, TAG_GYRO)
STORE_DELAY_FUNC(gyro, TAG_GYRO, CMD_CMN_INTERVAL_REQ)
static DEVICE_ATTR(gyro_setdelay, 0664, show_gyro_delay_result, attr_set_gyro_delay);

SHOW_ENABLE_FUNC(mag, TAG_MAG)
STORE_ENABLE_FUNC(mag, TAG_MAG, CMD_CMN_OPEN_REQ, CMD_CMN_CLOSE_REQ)
static DEVICE_ATTR(mag_enable, 0664, show_mag_enable_result, attr_set_mag_enable);
SHOW_DELAY_FUNC(mag, TAG_MAG)
STORE_DELAY_FUNC(mag, TAG_MAG, CMD_CMN_INTERVAL_REQ)
static DEVICE_ATTR(mag_setdelay, 0664, show_mag_delay_result, attr_set_mag_delay);

SHOW_ENABLE_FUNC(als, TAG_ALS)
STORE_ENABLE_FUNC(als, TAG_ALS, CMD_CMN_OPEN_REQ, CMD_CMN_CLOSE_REQ)
static DEVICE_ATTR(als_enable, 0664, show_als_enable_result, attr_set_als_enable);
SHOW_DELAY_FUNC(als, TAG_ALS)
STORE_DELAY_FUNC(als, TAG_ALS, CMD_CMN_INTERVAL_REQ)
static DEVICE_ATTR(als_setdelay, 0664, show_als_delay_result, attr_set_als_delay);

SHOW_ENABLE_FUNC(ps, TAG_PS)
STORE_ENABLE_FUNC(ps, TAG_PS, CMD_CMN_OPEN_REQ, CMD_CMN_CLOSE_REQ)
static DEVICE_ATTR(ps_enable, 0664, show_ps_enable_result, attr_set_ps_enable);
SHOW_DELAY_FUNC(ps, TAG_PS)
STORE_DELAY_FUNC(ps, TAG_PS, CMD_CMN_INTERVAL_REQ)
static DEVICE_ATTR(ps_setdelay, 0664, show_ps_delay_result, attr_set_ps_delay);

SHOW_ENABLE_FUNC(os, TAG_ORIENTATION)
STORE_ENABLE_FUNC(os, TAG_ORIENTATION, CMD_CMN_OPEN_REQ, CMD_CMN_CLOSE_REQ)
static DEVICE_ATTR(orientation_enable, 0664, show_os_enable_result, attr_set_os_enable);
SHOW_DELAY_FUNC(os, TAG_ORIENTATION)
STORE_DELAY_FUNC(os, TAG_ORIENTATION, CMD_CMN_INTERVAL_REQ)
static DEVICE_ATTR(orientation_setdelay, 0664, show_os_delay_result, attr_set_os_delay);

SHOW_ENABLE_FUNC(lines, TAG_LINEAR_ACCEL)
STORE_ENABLE_FUNC(lines, TAG_LINEAR_ACCEL, CMD_CMN_OPEN_REQ, CMD_CMN_CLOSE_REQ)
static DEVICE_ATTR(lines_enable, 0664, show_lines_enable_result, attr_set_lines_enable);
SHOW_DELAY_FUNC(lines, TAG_LINEAR_ACCEL)
STORE_DELAY_FUNC(lines, TAG_LINEAR_ACCEL, CMD_CMN_INTERVAL_REQ)
static DEVICE_ATTR(lines_setdelay, 0664, show_lines_delay_result, attr_set_lines_delay);

SHOW_ENABLE_FUNC(gras, TAG_GRAVITY)
STORE_ENABLE_FUNC(gras, TAG_GRAVITY, CMD_CMN_OPEN_REQ, CMD_CMN_CLOSE_REQ)
static DEVICE_ATTR(gras_enable, 0664, show_gras_enable_result, attr_set_gras_enable);
SHOW_DELAY_FUNC(gras, TAG_GRAVITY)
STORE_DELAY_FUNC(gras, TAG_GRAVITY, CMD_CMN_INTERVAL_REQ)
static DEVICE_ATTR(gras_setdelay, 0664, show_gras_delay_result, attr_set_gras_delay);

SHOW_ENABLE_FUNC(rvs, TAG_ROTATION_VECTORS)
STORE_ENABLE_FUNC(rvs, TAG_ROTATION_VECTORS, CMD_CMN_OPEN_REQ, CMD_CMN_CLOSE_REQ)
static DEVICE_ATTR(rvs_enable, 0664, show_rvs_enable_result, attr_set_rvs_enable);
SHOW_DELAY_FUNC(rvs, TAG_ROTATION_VECTORS)
STORE_DELAY_FUNC(rvs, TAG_ROTATION_VECTORS, CMD_CMN_INTERVAL_REQ)
static DEVICE_ATTR(rvs_setdelay, 0664, show_rvs_delay_result, attr_set_rvs_delay);

SHOW_ENABLE_FUNC(airpress, TAG_PRESSURE)
STORE_ENABLE_FUNC(airpress, TAG_PRESSURE, CMD_CMN_OPEN_REQ, CMD_CMN_CLOSE_REQ)
static DEVICE_ATTR(airpress_enable, 0664, show_airpress_enable_result, attr_set_airpress_enable);
SHOW_DELAY_FUNC(airpress, TAG_PRESSURE)
STORE_DELAY_FUNC(airpress, TAG_PRESSURE, CMD_CMN_INTERVAL_REQ)
static DEVICE_ATTR(airpress_setdelay, 0664, show_airpress_delay_result, attr_set_airpress_delay);

static int pdr_write_customize_cmd(struct write_info *pkg_ap)
{
	int ret;
	struct read_info pkg_mcu = { 0 };

	ret = write_customize_cmd(pkg_ap, &pkg_mcu, true);
	if (ret) {
		hwlog_err("pdr write cmd to mcu fail, ret=%d\n", ret);
		return -1;
	}
	if (pkg_mcu.errno != 0) {
		hwlog_err("set pdr delay fail\n");
		return -1;
	}

	return 0;
}

static ssize_t attr_set_pdr_delay(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	unsigned long val = 0;
	int start_update_flag;
	int precise;
	int interval;
	struct write_info pkg_ap = { 0 };
	pdr_ioctl_t pkg_ioctl;

	if (strict_strtoul(buf, TO_DECIMALISM, &val))
		return -EINVAL;
	/* val define: xyyzzz x:0 start 1 update yy:precise zzz:interval */
	if (val == 0)
		val = 1010; /* 1010: start, precise 1, interval 10 */
	start_update_flag = (val / 100000); /* 100000: to start or update */
	/* 1000: precise to unit */
	precise = ((val / 1000) %
		100); /* 100: just precise */
	interval = val % 1000; /* 1000: to interval */

	hwlog_info("val = %lu start_update_flag = %d precise = %d interval= %d\n",
		val, start_update_flag, precise, interval);
	if (precise == 0)
		precise = 1;
	if (interval == 0)
		interval = 240; /* interval 240 */

	pkg_ap.tag = TAG_PDR;
	pkg_ap.cmd = CMD_CMN_CONFIG_REQ;
	if (val >= 1000) { /* 1000: not only interval */
		pkg_ioctl.sub_cmd = (start_update_flag == 0) ?
			SUB_CMD_FLP_PDR_START_REQ : SUB_CMD_FLP_PDR_UPDATE_REQ;
		/* multiply 1000 */
		pkg_ioctl.start_param.report_interval = interval * 1000;
		/* multiply 1000 */
		pkg_ioctl.start_param.report_precise = precise * 1000;
		pkg_ioctl.start_param.report_count = interval / precise;
		pkg_ioctl.start_param.report_times = 0;
		pkg_ap.wr_buf = &pkg_ioctl;
		pkg_ap.wr_len = sizeof(pkg_ioctl);
	} else if (val == 2) { /* 2: stop command */
		pkg_ioctl.sub_cmd = SUB_CMD_FLP_PDR_STOP_REQ;
		pkg_ioctl.stop_param = 30000; /* stop param 30000 */
		pkg_ap.wr_buf = &pkg_ioctl;
		pkg_ap.wr_len = sizeof(pkg_ioctl);
	}
	hwlog_info(" pkg_ioctl.sub_cmd = %d\n ", pkg_ioctl.sub_cmd);
	if (pdr_write_customize_cmd(&pkg_ap) == 0)
		hwlog_info("set pdr delay (%ld)success\n", val);

	return size;
}

STORE_ENABLE_FUNC(pdr, TAG_PDR, CMD_CMN_OPEN_REQ, CMD_CMN_CLOSE_REQ)
static DEVICE_ATTR(pdr_enable, 0220, NULL, attr_set_pdr_enable);
static DEVICE_ATTR(pdr_setdelay, 0220, NULL, attr_set_pdr_delay);


SHOW_ENABLE_FUNC(handpress, TAG_HANDPRESS)
STORE_ENABLE_FUNC(handpress, TAG_HANDPRESS, CMD_CMN_OPEN_REQ, CMD_CMN_CLOSE_REQ)
static DEVICE_ATTR(handpress_enable, 0664, show_handpress_enable_result, attr_set_handpress_enable);
SHOW_DELAY_FUNC(handpress, TAG_HANDPRESS)
STORE_DELAY_FUNC(handpress, TAG_HANDPRESS, CMD_CMN_INTERVAL_REQ)
static DEVICE_ATTR(handpress_setdelay, 0664, show_handpress_delay_result, attr_set_handpress_delay);

SHOW_ENABLE_FUNC(cap_prox, TAG_CAP_PROX)
STORE_ENABLE_FUNC(cap_prox, TAG_CAP_PROX, CMD_CMN_OPEN_REQ, CMD_CMN_CLOSE_REQ)
static DEVICE_ATTR(cap_prox_enable, 0664, show_cap_prox_enable_result, attr_set_cap_prox_enable);
SHOW_DELAY_FUNC(cap_prox, TAG_CAP_PROX)
STORE_DELAY_FUNC(cap_prox, TAG_CAP_PROX, CMD_CMN_INTERVAL_REQ)
static DEVICE_ATTR(cap_prox_setdelay, 0664, show_cap_prox_delay_result, attr_set_cap_prox_delay);

SHOW_ENABLE_FUNC(magn_bracket, TAG_MAGN_BRACKET)
STORE_ENABLE_FUNC(magn_bracket, TAG_MAGN_BRACKET, CMD_CMN_OPEN_REQ, CMD_CMN_CLOSE_REQ)
static DEVICE_ATTR(magn_bracket_enable, 0664, show_magn_bracket_enable_result, attr_set_magn_bracket_enable);
SHOW_DELAY_FUNC(magn_bracket, TAG_MAGN_BRACKET)
STORE_DELAY_FUNC(magn_bracket, TAG_MAGN_BRACKET, CMD_CMN_INTERVAL_REQ)
static DEVICE_ATTR(magn_bracket_setdelay, 0664, show_magn_bracket_delay_result, attr_set_magn_bracket_delay);

static ssize_t start_iom3_recovery(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	hwlog_info("%s +\n", __func__);
	iom3_need_recovery(SENSORHUB_USER_MODID, SH_FAULT_USER_DUMP);
	hwlog_info("%s -\n", __func__);
	return size;
}
static DEVICE_ATTR(iom3_recovery, 0664, NULL, start_iom3_recovery);

static DEVICE_ATTR(sensor_test, 0660, NULL, attr_set_sensor_test_mode);

static DEVICE_ATTR(dt_motion_stup, 0664, NULL, attr_set_dt_motion_stup);

static DEVICE_ATTR(dt_stop_auto_data, 0664, NULL, attr_set_stop_auto_data);

static DEVICE_ATTR(dt_stop_auto_motion, 0660, attr_stop_auto_motion_show, attr_set_stop_auto_motion);

static ssize_t attr_set_stop_als_auto_data(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	g_stop_auto_als = simple_strtoul(buf, NULL, TO_HEXADECIMAL);
	hwlog_err("%s stop_auto_als %d\n", __func__, g_stop_auto_als);
	return size;
}
static DEVICE_ATTR(dt_stop_als_auto_data, 0664, NULL, attr_set_stop_als_auto_data);

static ssize_t attr_set_stop_ps_auto_data(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	g_stop_auto_ps = simple_strtoul(buf, NULL, TO_HEXADECIMAL);
	hwlog_err("%s stop_auto_ps %d\n", __func__, g_stop_auto_ps);
	return size;
}
static DEVICE_ATTR(dt_stop_ps_auto_data, 0664, NULL, attr_set_stop_ps_auto_data);

static ssize_t attr_set_sensor_motion_stup(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int i;
	unsigned long source;
	struct sensor_data event;

	source = simple_strtoul(buf, NULL, TO_HEXADECIMAL);
	if (source) {  /* 1: landscape */
		hwlog_err("%s landscape\n", __func__);
		event.type = TAG_ACCEL;
		event.length = 12; /* 12: int x 3 */
		event.value[0] = 1000; /* 1000: x axis val */
		event.value[1] = 0;
		event.value[2] = 0;
	} else {  /* 0: portial */
		hwlog_err("%s portial\n", __func__);
		event.type = TAG_ACCEL;
		event.length = 12; /* 12: int x 3 */
		event.value[0] = 0;
		event.value[1] = 1000; /* 1000: y axis val */
		event.value[2] = 0;
	}

	for (i = 0; i < 20; i++) { /* 20 times */
		msleep(100); /* sleep 100 ms */
		report_sensor_event(TAG_ACCEL, event.value, event.length);
	}
	return size;
}
static DEVICE_ATTR(dt_sensor_stup, 0664, NULL, attr_set_sensor_motion_stup);

static DEVICE_ATTR(dt_stepcounter_stup, 0664, NULL, attr_set_sensor_stepcounter_stup);

static ssize_t attr_set_dt_hall_sensor_stup(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct sensor_data event;
	unsigned long source;

	source = simple_strtoul(buf, NULL, TO_HEXADECIMAL);
	hwlog_err("%s buf %s, source %lu\n", __func__, buf, source);

	event.type = TAG_HALL;
	event.length = 4; /* 4 bytes */
	event.value[0] = (int)source;

	inputhub_route_write(ROUTE_SHB_PORT, (char *)&event, 8); /* 8 bytes */
	return size;
}
static DEVICE_ATTR(dt_hall_sensor_stup, 0664, NULL, attr_set_dt_hall_sensor_stup);

static ssize_t attr_set_dt_als_sensor_stup(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct sensor_data event;
	unsigned long source;

	source = simple_strtoul(buf, NULL, TO_HEXADECIMAL);
	hwlog_err("%s buf %s, source %lu\n", __func__, buf, source);

	event.type = TAG_ALS;
	event.length = 4; /* 4 bytes */
	event.value[0] = (int)source;

	inputhub_route_write(ROUTE_SHB_PORT, (char *)&event, 8); /* 8 bytes */
	return size;
}
static DEVICE_ATTR(dt_als_sensor_stup, 0664, NULL, attr_set_dt_als_sensor_stup);

static ssize_t attr_set_dt_ps_sensor_stup(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct sensor_data event;
	unsigned long source;

	source = simple_strtoul(buf, NULL, TO_HEXADECIMAL);
	hwlog_err("%s buf %s, source %lu\n", __func__, buf, source);

	event.type = TAG_PS;
	event.length = 4; /* 4 bytes */
	event.value[0] = (int)source;

	inputhub_route_write(ROUTE_SHB_PORT, (char *)&event, 8); /* 8 bytes */
	return size;
}
static DEVICE_ATTR(dt_ps_sensor_stup, 0664, NULL, attr_set_dt_ps_sensor_stup);

static ssize_t show_iom3_sr_status(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return snprintf_s(buf, MAX_STR_SIZE, MAX_STR_SIZE - 1, "%s\n",
		(get_iom3_sr_status() == ST_SLEEP) ? "ST_SLEEP" : "ST_WAKEUP");
}
static DEVICE_ATTR(iom3_sr_status, 0664, show_iom3_sr_status, NULL);

ssize_t show_cap_prox_calibrate_method(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return snprintf(buf, MAX_STR_SIZE, "%d\n", sar_pdata.calibrate_type);
}

ssize_t show_cap_prox_calibrate_orders(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	char *order_string = cap_prox_get_calibrate_order_string();

	if (!order_string)
		return -1;

	return snprintf_s(buf, MAX_STR_SIZE, MAX_STR_SIZE - 1, "%s\n",
		order_string);
}

ssize_t show_sensor_read_airpress_common(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	airpress_cali_flag = 1;
	return snprintf_s(buf, MAX_STR_SIZE, MAX_STR_SIZE - 1, "%d\n",
		inputhub_get_airpress_data());
}

static ssize_t show_sensor_read_airpress(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return show_sensor_read_airpress_common(dev, attr, buf);
}
static DEVICE_ATTR(read_airpress, 0664, show_sensor_read_airpress, NULL);

static ssize_t show_sensor_read_temperature(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return snprintf_s(buf, MAX_STR_SIZE, MAX_STR_SIZE - 1, "%d\n",
		inputhub_get_temperature_data());
}
static DEVICE_ATTR(read_temperature, 0664, show_sensor_read_temperature, NULL);

static ssize_t show_dump_sensor_status(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int tag;

	hwlog_info("-------------------------------------\n");
	for (tag = TAG_SENSOR_BEGIN; tag < TAG_SENSOR_END; ++tag) {
		if (unlikely((tag == TAG_PS) || (tag == TAG_STEP_COUNTER) ||
			(tag == TAG_MAGN_BRACKET))) /* ps and step counter need always on, just skip */
			continue;
		hwlog_info(" %s\t %s\t %d\n", obj_tag_str[tag],
			sensor_status.opened[tag] ? "open" : "close", sensor_status.delay[tag]);
	}
	hwlog_info("-------------------------------------\n");
	return snprintf_s(buf, MAX_STR_SIZE, MAX_STR_SIZE - 1,
		"please check log %d\n", inputhub_get_temperature_data());
}
static DEVICE_ATTR(dump_sensor_status, 0664, show_dump_sensor_status, NULL);

static ssize_t show_airpress_set_calidata(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	if (strlen(get_sensor_chip_info_address(AIRPRESS)) != 0)
		return snprintf(buf, MAX_STR_SIZE, "%d\n", airpress_data.offset);
	else
		return -1;
}

static ssize_t store_airpress_set_calidata(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	long source;
	int ret;
	int i;
	int temp;
	struct write_info pkg_ap = { 0 };
	struct read_info pkg_mcu = { 0 };
	pkt_parameter_req_t cpkt;
	struct pkt_header *hd = (struct pkt_header *)&cpkt;
	char content[CLI_CONTENT_LEN_MAX] = { 0 };
	char date_str[CLI_TIME_STR_LEN] = { 0 };

	if (strlen(get_sensor_chip_info_address(AIRPRESS)) == 0) {
		hwlog_err("AIRPRESS not exits !!\n");
		return -1;
	}
	if (!airpress_cali_flag) {
		hwlog_warn("Takes effect only when the calibration data\n");
		return -1;
	}
	source = simple_strtol(buf, NULL, TO_DECIMALISM);
	if (source > MAX_AIRPRESS_OFFSET || source < MIN_AIRPRESS_OFFSET) {
		hwlog_err("Incorrect offset. source = %d\n", source);
		return -1;
	}
	airpress_data.offset += (int)source;

	for (i = 0; i < 48; i++) /* max 48 */
		airpress_data.airpress_extend_data[i] = i;
	/* send to mcu */
	pkg_ap.tag = TAG_PRESSURE;
	pkg_ap.cmd = CMD_CMN_CONFIG_REQ;
	cpkt.subcmd = SUB_CMD_SET_OFFSET_REQ;
	pkg_ap.wr_buf = &hd[1];
	pkg_ap.wr_len = sizeof(airpress_data.offset) + SUBCMD_LEN;
	memcpy(cpkt.para, &airpress_data.offset, sizeof(airpress_data.offset));
	hwlog_info("***%s***\n", __func__);
	ret = write_customize_cmd(&pkg_ap, &pkg_mcu, true);
	if (ret != 0) {
		hwlog_err("set airpress_sensor data failed, ret = %d!\n", ret);
		return -1;
	}
	if (pkg_mcu.errno != 0)
		hwlog_err("set airpress_sensor sysfs offset fail,err=%d\n", pkg_mcu.errno);
	else
		hwlog_info("send airpress_sensor sysfs data :%d to mcu success\n", airpress_data.offset);

	get_test_time(date_str, sizeof(date_str));

	memset(&content, 0, sizeof(content));
	snprintf(content, CLI_CONTENT_LEN_MAX, PRESS_CALI_OFFSET,
		airpress_data.offset, "pass", "SUC", date_str);
	save_to_file(DATA_CLLCT, content);

	if (write_calibrate_data_to_nv(AIRPRESS_CALIDATA_NV_NUM, AIRPRESS_CALIDATA_NV_SIZE,
		"AIRDATA", (char *)&airpress_data.offset)) {
		hwlog_err("nve_direct_access write error %d\n", ret);
		return -1;
	}

	msleep(10); /* sleep 10 ms */
	if (read_calibrate_data_from_nv(AIRPRESS_CALIDATA_NV_NUM, AIRPRESS_CALIDATA_NV_SIZE, "AIRDATA")) {
		hwlog_err("nve direct access read error %d\n", ret);
		return -1;
	}
	memcpy(&temp, user_info.nv_data, sizeof(temp));
	if (temp != airpress_data.offset) {
		hwlog_err("nv write fail, %d %d\n", temp, airpress_data.offset);
		return -1;
	}
	airpress_cali_flag = 0;
	return size;
}
static DEVICE_ATTR(airpress_set_calidata, 0664, show_airpress_set_calidata,
	store_airpress_set_calidata);

static ssize_t show_gyro_set_calidata(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	if (!get_gyro_cali_way())
		return 0;
	if (strlen(get_sensor_chip_info_address(GYRO)) != 0)
		return snprintf(buf, MAX_STR_SIZE, "%d %d %d %d %d %d %d %d %d %d %d %d %d %d %d\n",
			set_gyro_calib_data[0], set_gyro_calib_data[1],
			set_gyro_calib_data[2], set_gyro_calib_data[3],
			set_gyro_calib_data[4], set_gyro_calib_data[5],
			set_gyro_calib_data[6], set_gyro_calib_data[7],
			set_gyro_calib_data[8], set_gyro_calib_data[9],
			set_gyro_calib_data[10], set_gyro_calib_data[11],
			set_gyro_calib_data[12], set_gyro_calib_data[13],
			set_gyro_calib_data[14]);
	else
		return -1;
}

static ssize_t store_gyro_set_calidata(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	const int32_t *set_gyro_cali_data = NULL;
	int i;
	int set_gyro_sensor_offset[GYRO_CALIBRATE_DATA_LENGTH];

	memset(set_gyro_sensor_offset, 0, sizeof(set_gyro_sensor_offset));
	if (!get_gyro_cali_way()) {
		hwlog_info("%s: now is gyro self_calibreate\n", __func__);
		return size;
	}
	if (strlen(get_sensor_chip_info_address(GYRO)) == 0) {
		hwlog_err("GYRO not exits !!\n");
		return -1;
	}
	set_gyro_cali_data = (const int32_t *)buf;
	if (size == sizeof(set_gyro_calib_data)) {
		set_gyro_calib_data[0] = *set_gyro_cali_data;
		set_gyro_calib_data[1] = *(set_gyro_cali_data + 1);
		set_gyro_calib_data[2] = *(set_gyro_cali_data + 2);
		set_gyro_calib_data[3] = *(set_gyro_cali_data + 3);
		set_gyro_calib_data[4] = *(set_gyro_cali_data + 4);
		set_gyro_calib_data[5] = *(set_gyro_cali_data + 5);
		set_gyro_calib_data[6] = *(set_gyro_cali_data + 6);
		set_gyro_calib_data[7] = *(set_gyro_cali_data + 7);
		set_gyro_calib_data[8] = *(set_gyro_cali_data + 8);
		set_gyro_calib_data[9] = *(set_gyro_cali_data + 9);
		set_gyro_calib_data[10] = *(set_gyro_cali_data + 10);
		set_gyro_calib_data[11] = *(set_gyro_cali_data + 11);
		set_gyro_calib_data[12] = *(set_gyro_cali_data + 12);
		set_gyro_calib_data[13] = *(set_gyro_cali_data + 13);
		set_gyro_calib_data[14] = *(set_gyro_cali_data + 14);
	} else {
		hwlog_err("%s:size %d is not equal to 15*4.\n",  __func__, size);
		return -1;
	}
	for (i = 0; i < CALIBRATE_DATA_LENGTH; i++) {
		if (set_gyro_calib_data[i] < gyro_calib_threshold[i].low_threshold ||
			set_gyro_calib_data[i] > gyro_calib_threshold[i].high_threshold) {
			hwlog_err("%s: gyro calibrated_data is out of range. i = %d, num = %d.\n",
				__func__, i, set_gyro_calib_data[i]);
			return -1;
		}
	}
	gyro_calibrate_save(set_gyro_calib_data, sizeof(set_gyro_calib_data));
	hwlog_info("set gyro calibrate success, data=%d %d %d %d %d %d %d %d %d %d %d %d %d %d %d\n",
			set_gyro_calib_data[0], set_gyro_calib_data[1],
			set_gyro_calib_data[2], set_gyro_calib_data[3],
			set_gyro_calib_data[4], set_gyro_calib_data[5],
			set_gyro_calib_data[6], set_gyro_calib_data[7],
			set_gyro_calib_data[8], set_gyro_calib_data[9],
			set_gyro_calib_data[10], set_gyro_calib_data[11],
			set_gyro_calib_data[12], set_gyro_calib_data[13],
			set_gyro_calib_data[14]);
	if (read_calibrate_data_from_nv(GYRO_CALIDATA_NV_NUM, GYRO_CALIDATA_NV_SIZE, "GYRO"))
		return -1;

	/* copy to gsensor_offset by pass */
	memcpy(set_gyro_sensor_offset, user_info.nv_data, sizeof(set_gyro_sensor_offset));
	hwlog_info("nve_direct_access read gyro_sensor offset: %d %d %d  sensitity:%d %d %d\n",
		set_gyro_sensor_offset[0], set_gyro_sensor_offset[1],
		set_gyro_sensor_offset[2], set_gyro_sensor_offset[3],
		set_gyro_sensor_offset[4], set_gyro_sensor_offset[5]);
	hwlog_info("nve_direct_access read gyro_sensor xis_angle: %d %d %d  %d %d %d %d %d %d\n",
		set_gyro_sensor_offset[6], set_gyro_sensor_offset[7],
		set_gyro_sensor_offset[8], set_gyro_sensor_offset[9],
		set_gyro_sensor_offset[10], set_gyro_sensor_offset[11],
		set_gyro_sensor_offset[12], set_gyro_sensor_offset[13],
		set_gyro_sensor_offset[14]);
	if (send_calibrate_data_to_mcu(TAG_GYRO, SUB_CMD_SET_OFFSET_REQ,
		set_gyro_sensor_offset, GYRO_CALIDATA_NV_SIZE, false))
		return -1;
	return size;
}
static DEVICE_ATTR(gyro_set_calidata, 0660, show_gyro_set_calidata, store_gyro_set_calidata);

static ssize_t show_acc_set_calidata(struct device *dev, struct device_attribute *attr, char *buf)
{
	if (!get_acc_cali_way())
		return 0;
	if (strlen(get_sensor_chip_info_address(ACC)) != 0)
		return snprintf(buf, MAX_STR_SIZE, "%d %d %d %d %d %d %d %d %d %d %d %d %d %d %d\n",
			set_acc_calib_data[0], set_acc_calib_data[1],
			set_acc_calib_data[2], set_acc_calib_data[3],
			set_acc_calib_data[4], set_acc_calib_data[5],
			set_acc_calib_data[6], set_acc_calib_data[7],
			set_acc_calib_data[8], set_acc_calib_data[9],
			set_acc_calib_data[10], set_acc_calib_data[11],
			set_acc_calib_data[12], set_acc_calib_data[13],
			set_acc_calib_data[14]);
	else
		return -1;
}

static ssize_t store_acc_set_calidata(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	const int32_t *set_acc_cali_data = NULL;
	int set_acc_sensor_offset[ACC_CALIBRATE_DATA_LENGTH];
	int i;

	memset(set_acc_sensor_offset, 0, sizeof(set_acc_sensor_offset));
	if (!get_acc_cali_way()) {
		hwlog_info("%s: now is acc self_calibreate\n", __func__);
		return size;
	}
	if (strlen(get_sensor_chip_info_address(ACC)) == 0) {
		hwlog_err("ACC not exits !!\n");
		return -1;
	}
	set_acc_cali_data = (const int32_t *)buf;
	if (size == sizeof(set_gyro_calib_data)) {
		set_acc_calib_data[0] = *set_acc_cali_data;
		set_acc_calib_data[1] = *(set_acc_cali_data + 1);
		set_acc_calib_data[2] = *(set_acc_cali_data + 2);
		set_acc_calib_data[3] = *(set_acc_cali_data + 3);
		set_acc_calib_data[4] = *(set_acc_cali_data + 4);
		set_acc_calib_data[5] = *(set_acc_cali_data + 5);
		set_acc_calib_data[6] = *(set_acc_cali_data + 6);
		set_acc_calib_data[7] = *(set_acc_cali_data + 7);
		set_acc_calib_data[8] = *(set_acc_cali_data + 8);
		set_acc_calib_data[9] = *(set_acc_cali_data + 9);
		set_acc_calib_data[10] = *(set_acc_cali_data + 10);
		set_acc_calib_data[11] = *(set_acc_cali_data + 11);
		set_acc_calib_data[12] = *(set_acc_cali_data + 12);
		set_acc_calib_data[13] = *(set_acc_cali_data + 13);
		set_acc_calib_data[14] = *(set_acc_cali_data + 14);
	} else {
		hwlog_err("%s:size %d is not equal to 15*4.\n",  __func__, size);
		return -1;
	}
	for (i = 0; i < CALIBRATE_DATA_LENGTH; i++) {
		if (set_acc_calib_data[i] < acc_calib_threshold[i].low_threshold ||
			set_acc_calib_data[i] > acc_calib_threshold[i].high_threshold) {
			hwlog_err("%s: acc calibrated_data is out of range. i = %d, num = %d.\n",
				__func__, i, set_acc_calib_data[i]);
			return -1;
		}
	}
	acc_calibrate_save(set_acc_calib_data, sizeof(set_acc_calib_data));
	hwlog_info("set acc calibrate success, data=%d %d %d %d %d %d %d %d %d %d %d %d %d %d %d\n",
			set_acc_calib_data[0], set_acc_calib_data[1],
			set_acc_calib_data[2], set_acc_calib_data[3],
			set_acc_calib_data[4], set_acc_calib_data[5],
			set_acc_calib_data[6], set_acc_calib_data[7],
			set_acc_calib_data[8], set_acc_calib_data[9],
			set_acc_calib_data[10], set_acc_calib_data[11],
			set_acc_calib_data[12], set_acc_calib_data[13],
			set_acc_calib_data[14]);
	if (read_calibrate_data_from_nv(ACC_OFFSET_NV_NUM, ACC_OFFSET_NV_SIZE, "gsensor"))
		return -1;

	/* copy to gsensor_offset by pass */
	memcpy(set_acc_sensor_offset, user_info.nv_data, sizeof(set_acc_sensor_offset));
	hwlog_info("nve_direct_access read gyro_sensor offset: %d %d %d  sensitity:%d %d %d\n",
		set_acc_sensor_offset[0], set_acc_sensor_offset[1],
		set_acc_sensor_offset[2], set_acc_sensor_offset[3],
		set_acc_sensor_offset[4], set_acc_sensor_offset[5]);
	hwlog_info("nve_direct_access read gyro_sensor xis_angle: %d %d %d  %d %d %d %d %d %d\n",
		set_acc_sensor_offset[6], set_acc_sensor_offset[7],
		set_acc_sensor_offset[8], set_acc_sensor_offset[9],
		set_acc_sensor_offset[10], set_acc_sensor_offset[11],
		set_acc_sensor_offset[12], set_acc_sensor_offset[13],
		set_acc_sensor_offset[14]);

	if (send_calibrate_data_to_mcu(TAG_ACCEL, SUB_CMD_SET_OFFSET_REQ,
		set_acc_sensor_offset, ACC_OFFSET_NV_SIZE, false))
		return -1;
	return size;
}
static DEVICE_ATTR(acc_set_calidata, 0660, show_acc_set_calidata, store_acc_set_calidata);

static ssize_t store_set_data_type(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int32_t set_data_type[2];
	const int32_t *set_type = NULL;
	struct write_info pkg_ap = { 0 };
	struct read_info pkg_mcu = { 0 };
	pkt_parameter_req_t spkt;
	struct pkt_header *shd = (struct pkt_header *)&spkt;
	int ret;

	memset(&set_data_type, 0, sizeof(set_data_type));
	memset(&spkt, 0, sizeof(spkt));
	set_type = (const int32_t *)buf;
	if (size == sizeof(set_data_type)) {
		set_data_type[0] = *set_type;
		set_data_type[1] = *(set_type + 1);
	} else {
		hwlog_err("%s:size %d is less than 8.\n", __func__, size);
		return -1;
	}
	if (!get_acc_cali_way()) {
		hwlog_info("%s: now is self_calibreate\n", __func__);
		return size;
	}
	hwlog_info("%s: data type tag is %d (1.acc2.gyro),type is %d(1.raw_data2.cali_data4.nor_data)\n",
		__func__, set_data_type[0], set_data_type[1]);
	if (set_data_type[0] < TAG_ACCEL || set_data_type[0] > TAG_GYRO) {
		hwlog_err("%s:set sensor tag is fail, invalid val\n", __func__);
		return -1;
	}
	if (set_data_type[1] < 1 || set_data_type[1] > 4) { /* max 4 */
		hwlog_err("%s:set data type is fail, invalid val\n", __func__);
		return -1;
	}
	if (set_data_type[0] == TAG_ACCEL)
		pkg_ap.tag = TAG_ACCEL;
	if (set_data_type[0] == TAG_GYRO)
		pkg_ap.tag = TAG_GYRO;
	spkt.subcmd = SUB_CMD_SET_DATA_TYPE_REQ;

	pkg_ap.cmd = CMD_CMN_CONFIG_REQ;
	pkg_ap.wr_buf = &shd[1];
	pkg_ap.wr_len = sizeof(set_data_type[1]) + SUBCMD_LEN;
	memcpy(spkt.para, &set_data_type[1], sizeof(set_data_type[1]));
	ret = write_customize_cmd(&pkg_ap, &pkg_mcu, true);
	if (pkg_mcu.errno != 0)
		hwlog_err("send tag %d get diff data cmd to mcu fail,ret=%d\n", set_data_type[0], ret);
	return size;
}
static DEVICE_ATTR(set_data_type, 0220, NULL, store_set_data_type);

static ssize_t attr_handpress_calibrate_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int result = (handpress_calibration_res == SUC) ? 0 : handpress_calibration_res;

	return snprintf(buf, PAGE_SIZE, "%d\n", result);
}

int write_handpress_offset_to_nv(const void *offset)
{
	int8_t offset2[24] = {0};

	if (!offset) {
		hwlog_err("%s fail, invalid para!\n", __func__);
		return -1;
	}
	memcpy(offset2, offset, HANDPRESS_CALIDATA_NV_SIZE);
	if (write_calibrate_data_to_nv(HANDPRESS_CALIDATA_NV_NUM, HANDPRESS_CALIDATA_NV_SIZE, "HPDATA", offset2))
		return -1;

	if (read_calibrate_data_from_nv(HANDPRESS_CALIDATA_NV_NUM, HANDPRESS_CALIDATA_NV_SIZE, "HPDATA"))
		return -1;
	memset(offset2, 0, sizeof(offset2));
	memcpy(offset2, user_info.nv_data, sizeof(offset2));
	hwlog_err("offsets: %d%d%d%d%d%d%d%d\n", offset2[0], offset2[1], offset2[2],
		offset2[3], offset2[4], offset2[5], offset2[6], offset2[7]);
	return 0;
}

static void handpress_calibrate_work_func(struct work_struct *work)
{
	int ret;

	hwlog_err("handpress calibrate work enter ++\n");
	ret = write_handpress_offset_to_nv(g_hp_offset);
	if (ret < 0)  {
		hwlog_err("nv write faild.\n");
		handpress_calibration_res = NV_FAIL;
	}
	hwlog_err("handpress calibrate work enter --\n");
}

static ssize_t attr_handpress_calibrate_write(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned long val = 0;
	struct read_info pkg_mcu;
	int data_len;
	char content[CLI_CONTENT_LEN_MAX] = { 0 };
	char date_str[CLI_TIME_STR_LEN] = { 0 };
	uint16_t hand_offset[8] = { 0 };
	uint16_t hand_metal[8] = { 0 };
	uint8_t hand_idac1[8] = { 0 };
	uint8_t hand_calidata[24] = { 0 };

	handpress_calibration_res = EXEC_FAIL;

	if (strict_strtoul(buf, TO_DECIMALISM, &val))
		return -EINVAL;

	if (strlen(get_sensor_chip_info_address(HANDPRESS)) == 0) {
		hwlog_err("no handpress\n");
		return count;
	}

	if (val == 1) {
		if ((hall_value & 0x01) != 0x01) {
			hwlog_err("hall value:%d\n", hall_value);
			handpress_calibration_res = POSITION_FAIL;
			return count;
		}
	}
	pkg_mcu = send_calibrate_cmd(TAG_HANDPRESS, val, &handpress_calibration_res);
	if (handpress_calibration_res == COMMU_FAIL || handpress_calibration_res == EXEC_FAIL)
		goto hp_cali_out;

	hwlog_err("data_len:%d\n", pkg_mcu.data_length);
	data_len = sizeof(hand_calidata);
	data_len = (pkg_mcu.data_length > data_len) ? data_len :
		pkg_mcu.data_length;
	memcpy(hand_calidata, pkg_mcu.data, data_len);
	if (memcpy_s(g_hp_offset, sizeof(g_hp_offset), hand_calidata,
		sizeof(g_hp_offset)) != EOK)
		return -1;
	handpress_calibration_res = SUC;
	if (val == 1) {
		INIT_WORK(&handpress_calibrate_work, handpress_calibrate_work_func);
		queue_work(system_power_efficient_wq, &handpress_calibrate_work);
		msleep(50); /* sleep 50 ms */
		/* 8: hand_offset start form g_hp_offset[8] */
		if (memcpy_s(hand_offset, sizeof(hand_offset), &g_hp_offset[8],
			sizeof(hand_offset)) != EOK)
			return -1;
		if (memcpy_s(hand_idac1, sizeof(hand_idac1), g_hp_offset,
			sizeof(hand_idac1)) != EOK)
			return -1;
		get_test_time(date_str, sizeof(date_str));
		snprintf(content, CLI_CONTENT_LEN_MAX, "info 1:\ntime:%s\noffset:%d %d %d %d %d %d %d %d\nidac2:%d %d %d %d %d %d %d %d\n",
			date_str, hand_offset[0], hand_offset[1], hand_offset[2],
			hand_offset[3], hand_offset[4], hand_offset[5], hand_offset[6],
			hand_offset[7], hand_idac1[0], hand_idac1[1], hand_idac1[2],
			hand_idac1[3], hand_idac1[4], hand_idac1[5], hand_idac1[6], hand_idac1[7]);
			hwlog_info("time:%s\noffset:%d %d %d %d %d %d %d %d\nidac2:%d %d %d %d %d %d %d %d\n",
			date_str, hand_offset[0], hand_offset[1], hand_offset[2],
			hand_offset[3], hand_offset[4], hand_offset[5], hand_offset[6],
			hand_offset[7], hand_idac1[0], hand_idac1[1], hand_idac1[2],
			hand_idac1[3], hand_idac1[4], hand_idac1[5], hand_idac1[6], hand_idac1[7]);
	} else if (val == 2) { /* 2: print hand_calidata */
		data_len = sizeof(hand_metal);
		memcpy(hand_metal, hand_calidata, data_len);
		snprintf(content, CLI_CONTENT_LEN_MAX, "info 2:\nmetal:%d %d %d %d %d %d %d %d\n",
			hand_metal[0], hand_metal[1], hand_metal[2], hand_metal[3],
			hand_metal[4], hand_metal[5], hand_metal[6], hand_metal[7]);
		hwlog_info("metal:%d %d %d %d %d %d %d %d\n",
			hand_metal[0], hand_metal[1], hand_metal[2], hand_metal[3],
			hand_metal[4], hand_metal[5], hand_metal[6], hand_metal[7]);
	} else {
		hwlog_err("invalid input:%d\n", (int)val);
		snprintf(content, CLI_CONTENT_LEN_MAX, "invalid input:%d\n", (int)val);
	}
	save_to_file(HAND_DATA_CLLCT, content);
hp_cali_out:
	return count;
}
static DEVICE_ATTR(handpress_calibrate, 0664, attr_handpress_calibrate_show,
	attr_handpress_calibrate_write);

ssize_t sensors_calibrate_show(int tag, struct device *dev,
	struct device_attribute *attr, char *buf)
{
	switch (tag) {
	case TAG_ACCEL:
		return snprintf(buf, PAGE_SIZE, "%d\n", return_calibration != SUC); /* flyhorse k: SUC-->"0", OTHERS-->"1" */

	case TAG_PS:
		return snprintf(buf, PAGE_SIZE, "%d\n", ps_calibration_res != SUC); /* flyhorse k: SUC-->"0", OTHERS-->"1" */

	case TAG_ALS:
		return snprintf(buf, PAGE_SIZE, "%d\n", als_calibration_res != SUC); /* flyhorse k: SUC-->"0", OTHERS-->"1" */

	case TAG_GYRO:
		return snprintf(buf, PAGE_SIZE, "%d\n", gyro_calibration_res != SUC); /* flyhorse k: SUC-->"0", OTHERS-->"1" */

	case TAG_PRESSURE:
		return show_airpress_set_calidata(dev, attr, buf);

	case TAG_HANDPRESS:
		return snprintf(buf, PAGE_SIZE, "%d\n", handpress_calibration_res != SUC);

	case TAG_CAP_PROX:
		return snprintf(buf, PAGE_SIZE, "%d\n", return_cap_prox_calibration != SUC);

	default:
		hwlog_err("tag %d calibrate not implement in %s\n", tag, __func__);
		break;
	}

	return 0;
}

ssize_t sensors_calibrate_store(int tag, struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	switch (tag) {
	case TAG_ACCEL:
		return attr_acc_calibrate_write(dev, attr, buf, count);

	case TAG_PS:
		return attr_ps_calibrate_write(dev, attr, buf, count);

	case TAG_ALS:
		return attr_als_calibrate_write(dev, attr, buf, count);

	case TAG_GYRO:
		return attr_gyro_calibrate_write(dev, attr, buf, count);

	case TAG_PRESSURE:
		return store_airpress_set_calidata(dev, attr, buf, count);

	case TAG_HANDPRESS:
		return attr_handpress_calibrate_write(dev, attr, buf, count);

	case TAG_CAP_PROX:
		return attr_cap_prox_calibrate_write(dev, attr, buf, count);

	default:
		hwlog_err("tag %d calibrate not implement in %s\n", tag, __func__);
		break;
	}

	return count;
}

int ois_commu(int tag, unsigned int cmd, unsigned int pare,
	unsigned int responsed, bool is_subcmd)
{
	int ret;
	struct write_info pkg_ap = { 0 };
	struct read_info pkg_mcu = { 0 };
	pkt_parameter_req_t cpkt;
	struct pkt_header *hd = (struct pkt_header *)&cpkt;

	if (is_subcmd) {
		pkg_ap.tag = tag;
		pkg_ap.cmd = CMD_CMN_CONFIG_REQ;
		cpkt.subcmd = cmd;
		pkg_ap.wr_buf = &hd[1];
		pkg_ap.wr_len = sizeof(pare) + SUBCMD_LEN;
		memcpy(cpkt.para, &pare, sizeof(pare));
	} else {
		pkg_ap.tag = tag;
		pkg_ap.cmd = cmd;
		pkg_ap.wr_buf = &pare;
		pkg_ap.wr_len = sizeof(pare);
	}

	if (responsed == NO_RESP) {
		ret = write_customize_cmd(&pkg_ap, NULL, true);
		if (ret) {
			hwlog_err("send ois cmd %d to mcu fail,ret=%d\n", cmd, ret);
			return ret;
		}
	} else {
		ret = write_customize_cmd(&pkg_ap, &pkg_mcu, true);
		if (ret) {
			hwlog_err("send ois gyro cfg cmd failed, ret = %d!\n", ret);
			return ret;
		}
		if (pkg_mcu.errno != 0) {
			hwlog_err("set ois gyro cfg cmd fail,err=%d\n", pkg_mcu.errno);
		} else {
			hwlog_info("set ois gyro cfg cmd %d success\n", pare);
			sensor_status.gyro_ois_status = pare;
		}
	}

	return ret;
}

static ssize_t show_ois_ctrl(struct device *dev, struct device_attribute *attr, char *buf)
{
	return snprintf(buf, MAX_STR_SIZE, "%d\n", sensor_status.gyro_ois_status);
}

static ssize_t store_ois_ctrl(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	long source;
	int ret;
	unsigned int cmd;
	unsigned int delay = 10;

	source = simple_strtol(buf, NULL, TO_DECIMALISM);
	if (source == sensor_status.gyro_ois_status) {
		hwlog_info("%s:gyro ois status unchange,source=%ld return\n", __func__, source);
		return size;
	}

	if (source == 1) { /* 1: open, set interval, enable(RESP) */
		cmd = CMD_CMN_OPEN_REQ;
		ret = ois_commu(TAG_OIS, cmd, source, NO_RESP, false);
		if (ret) {
			hwlog_err("%s: ois open gyro fail\n", __func__);
			return size;
		}

		cmd = CMD_CMN_INTERVAL_REQ;
		ret = ois_commu(TAG_OIS, cmd, delay, NO_RESP, false);
		if (ret) {
			hwlog_err("%s: set delay fail\n", __func__);
			return size;
		}

		cmd = SUB_CMD_GYRO_OIS_REQ;
		ret = ois_commu(TAG_GYRO, cmd, source, RESP, true);
		if (ret) {
			hwlog_err("%s: ois enable fail\n", __func__);
			return size;
		}
		hwlog_info("%s:ois enable succsess\n", __func__);
	} else if (source == 0) { /* 0: disable(RESP), close */
		cmd = SUB_CMD_GYRO_OIS_REQ;
		ret = ois_commu(TAG_GYRO, cmd, source, RESP, true);
		if (ret) {
			hwlog_err("%s:ois close fail\n", __func__);
			return size;
		}

		cmd = CMD_CMN_CLOSE_REQ;
		ret = ois_commu(TAG_OIS, cmd, source, NO_RESP, false);
		if (ret) {
			hwlog_err("%s: ois close gyro fail\n", __func__);
			return size;
		}
		hwlog_info("%s:ois close succsess\n", __func__);
	} else if (source == 2) { /* 2: reset(RESP) */
		cmd = SUB_CMD_GYRO_OIS_REQ;
		ret = ois_commu(TAG_GYRO, cmd, source, RESP, true);
		if (ret) {
			hwlog_err("%s: ois enable fail\n", __func__);
			return size;
		}
		hwlog_info("%s:ois reset succsess\n", __func__);
	} else if (source == 3) { /* 3: open, set interval, enable(NO_RESP) */
		source = 1;
		cmd = CMD_CMN_OPEN_REQ;
		ret = ois_commu(TAG_OIS, cmd, source, NO_RESP, false);
		if (ret) {
			hwlog_err("%s: ois open gyro fail\n", __func__);
			return size;
		}

		cmd = CMD_CMN_INTERVAL_REQ;
		ret = ois_commu(TAG_OIS, cmd, delay, NO_RESP, false);
		if (ret) {
			hwlog_err("%s: set delay fail\n", __func__);
			return size;
		}
		cmd = SUB_CMD_GYRO_OIS_REQ;
		ret = ois_commu(TAG_GYRO, cmd, source, NO_RESP, true);
		if (ret) {
			hwlog_err("%s: ois enable no_resp fail\n", __func__);
			return size;
		}
		hwlog_info("%s:ois enable succsess\n", __func__);
	} else {
		hwlog_info("%s:ois commend is not right\n", __func__);
	}
	return size;
}
static DEVICE_ATTR(ois_ctrl, 0664, show_ois_ctrl, store_ois_ctrl);

static ssize_t show_key_debug(struct device *dev, struct device_attribute *attr, char *buf)
{
	return snprintf(buf, MAX_STR_SIZE, "%d\n", sensor_status.gyro_ois_status);
}

static int send_offset_cmd(uint8_t tag, uint32_t subcmd, int value)
{
	struct write_info pkg_ap = { 0 };
	struct read_info pkg_mcu = { 0 };
	pkt_parameter_req_t cpkt;
	struct pkt_header *hd = (struct pkt_header *)&cpkt;
	int ret;

	pkg_ap.cmd = CMD_CMN_CONFIG_REQ;
	pkg_ap.tag = tag;
	cpkt.subcmd = subcmd;
	pkg_ap.wr_buf = &hd[1];
	pkg_ap.wr_len = sizeof(value) + SUBCMD_LEN;
	memcpy(cpkt.para, &value, sizeof(value));
	ret = write_customize_cmd(&pkg_ap, &pkg_mcu, true);
	if (ret < 0) {
		hwlog_err("err. write cmd\n");
		return -1;
	}

	if (pkg_mcu.errno != 0) {
		hwlog_info("mcu err\n");
		return -1;
	} else {
		return 0;
	}
}

static ssize_t store_key_debug(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int source;
	int ret;
	int value;
	int offset_value;
	interval_param_t param;

	if (strlen(get_sensor_chip_info_address(KEY)) == 0) {
		hwlog_err("no key\n");
		return size;
	}

	source = simple_strtol(buf, NULL, TO_DECIMALISM);
	value = ((source>>8) & 0xff);
	offset_value = (source & 0xff);
	hwlog_info("value:%d  offset:%d\n", value, offset_value);
	switch (value) {
	case 0:
		ret = inputhub_sensor_enable(TAG_KEY, false);
		break;
	case 1:
		ret = inputhub_sensor_enable(TAG_KEY, true);
		break;
	case 2: /* 2 or 5: set offset */
	case 5: /* 2 or 5: set offset */
		ret = send_offset_cmd(TAG_KEY, SUB_CMD_SET_OFFSET_REQ, offset_value);
		break;
	case 3: /* 3: set interval */
		memset(&param, 0, sizeof(param));
		param.period = 500; /* period 500 ms */
		ret = inputhub_sensor_setdelay(TAG_KEY, &param);
		break;
	case 4: /* 4: set backlight */
		ret = send_offset_cmd(TAG_KEY, SUB_CMD_BACKLIGHT_REQ, offset_value);
		break;
	default:
		return -1;
	}

	if (!ret)
		return size;
	else
		return -1;
}
static DEVICE_ATTR(key_debug, 0664, show_key_debug, store_key_debug);

static ssize_t show_sar_data(struct device *dev, struct device_attribute *attr, char *buf)
{
	char *chip_info = get_sensor_chip_info_address(CAP_PROX);

	if (!strncmp(chip_info, "huawei,cypress_sar_psoc4000",
		strlen("huawei,cypress_sar_psoc4000"))) {
		return snprintf(buf, MAX_STR_SIZE, "idac:%d rawdata:%d near:%d far:%d\n",
			sar_calibrate_datas.cypres_cali_data.sar_idac,
			sar_calibrate_datas.cypres_cali_data.raw_data,
			sar_calibrate_datas.cypres_cali_data.near_signaldata,
			sar_calibrate_datas.cypres_cali_data.far_signaldata);
	}
	if (!strncmp(chip_info, "huawei,semtech-sx9323",
		strlen("huawei,semtech-sx9323"))) {
		return snprintf(buf, MAX_STR_SIZE, "offset:%d diff:%d\n",
			sar_calibrate_datas.semtech_cali_data.offset,
			sar_calibrate_datas.semtech_cali_data.diff);
	}
	if (!strncmp(chip_info, "huawei,abov-a96t3x6",
		strlen("huawei,abov-a96t3x6"))) {
		return snprintf(buf, MAX_STR_SIZE, "offset[0]:%d, offset[1]:%d, diff[0]:%d, diff[1]:%d\n",
			sar_calibrate_datas.abov_cali_data.offset[SAR_ABOV_CH_0],
			sar_calibrate_datas.abov_cali_data.offset[SAR_ABOV_CH_1],
			sar_calibrate_datas.abov_cali_data.diff[SAR_ABOV_CH_0],
			sar_calibrate_datas.abov_cali_data.diff[SAR_ABOV_CH_1]);
	}
	if (!strncmp(chip_info, "huawei,awi-aw9610x",
		strlen("huawei,awi-aw9610x"))) {
		return snprintf(buf, MAX_STR_SIZE, "offset:%d diff:%d\n",
			sar_calibrate_datas.aw9610_cali_data.offset,
			sar_calibrate_datas.aw9610_cali_data.diff);
	}

	return -1;
}
static DEVICE_ATTR(sar_data, 0444, show_sar_data, NULL);

static ssize_t show_hifi_supported(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return snprintf_s(buf, MAX_STR_SIZE, MAX_STR_SIZE - 1, "%d\n",
		get_hifi_supported());
}
static DEVICE_ATTR(hifi_supported, 0664, show_hifi_supported, NULL);

static enum detect_state sensor_detect_flag = DET_FAIL;
static ssize_t show_sensor_detect(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct sensor_detect_manager *sensor_manager = get_sensor_manager();
	int i;
	int detect_result = 0;

	for (i = 0; i < SENSOR_MAX; i++) {
		detect_result = sensor_manager[i].detect_result;
		if (detect_result == sensor_detect_flag)
			if (snprintf_s(&buf[i * MAX_SENSOR_NAME_LENGTH],
				MAX_SENSOR_NAME_LENGTH,
				MAX_SENSOR_NAME_LENGTH - 1, "%s ",
				sensor_manager[i].sensor_name_str) < 0)
				hwlog_err("%s error i=%d\n", __func__, i);
	}

	return MAX_STR_SIZE;
}

static ssize_t store_sensor_detect(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int flag;

	flag = simple_strtol(buf, NULL, TO_DECIMALISM);
	sensor_detect_flag = flag ? DET_SUCC : DET_FAIL;
	hwlog_info("sensor detect value %d\n", (int)sensor_detect_flag);

	return size;
}
static DEVICE_ATTR(sensor_detect, 0660, show_sensor_detect, store_sensor_detect);

static struct attribute *sensor_attributes[] = {
	&dev_attr_acc_info.attr,
	&dev_attr_mag_info.attr,
	&dev_attr_gyro_info.attr,
	&dev_attr_ps_info.attr,
	&dev_attr_als_info.attr,
	&dev_attr_acc_read_data.attr,
	&dev_attr_mag_read_data.attr,
	&dev_attr_gyro_read_data.attr,
	&dev_attr_ps_read_data.attr,
	&dev_attr_als_read_data.attr,
	&dev_attr_gyro_selfTest.attr,
	&dev_attr_mag_selfTest.attr,
	&dev_attr_acc_selfTest.attr,
	&dev_attr_gps_4774_i2c_selfTest.attr,
	&dev_attr_i2c_rw.attr,
	&dev_attr_i2c_rw16.attr,
	&dev_attr_acc_calibrate.attr,
	&dev_attr_acc_enable.attr,
	&dev_attr_acc_setdelay.attr,
	&dev_attr_acc_set_calidata.attr,
	&dev_attr_set_data_type.attr,
	&dev_attr_set_fingersense_enable.attr,
	&dev_attr_fingersense_req_data.attr,
	&dev_attr_fingersense_data_ready.attr,
	&dev_attr_fingersense_latch_data.attr,
	&dev_attr_gsensor_gather_enable.attr,
	&dev_attr_gyro_calibrate.attr,
	&dev_attr_gyro_enable.attr,
	&dev_attr_gyro_setdelay.attr,
	&dev_attr_gyro_set_calidata.attr,
	&dev_attr_mag_enable.attr,
	&dev_attr_mag_setdelay.attr,
	&dev_attr_als_calibrate.attr,
	&dev_attr_als_enable.attr,
	&dev_attr_als_setdelay.attr,
	&dev_attr_ps_calibrate.attr,
	&dev_attr_ps_enable.attr,
	&dev_attr_ps_setdelay.attr,
	&dev_attr_ps_switch_mode.attr,
	&dev_attr_ps_support_mode.attr,
	&dev_attr_pdr_enable.attr,
	&dev_attr_pdr_setdelay.attr,
	&dev_attr_orientation_enable.attr,
	&dev_attr_orientation_setdelay.attr,
	&dev_attr_lines_enable.attr,
	&dev_attr_lines_setdelay.attr,
	&dev_attr_gras_enable.attr,
	&dev_attr_gras_setdelay.attr,
	&dev_attr_rvs_enable.attr,
	&dev_attr_rvs_setdelay.attr,
	&dev_attr_sensor_list_info.attr,
	&dev_attr_iom3_recovery.attr,
	&dev_attr_sensor_test.attr,
	&dev_attr_iom3_sr_test.attr,
	&dev_attr_dt_motion_stup.attr,
	&dev_attr_dt_sensor_stup.attr,
	&dev_attr_dt_stop_auto_data.attr,
	&dev_attr_dt_hall_sensor_stup.attr,
	&dev_attr_dt_stop_als_auto_data.attr,
	&dev_attr_dt_als_sensor_stup.attr,
	&dev_attr_dt_stop_ps_auto_data.attr,
	&dev_attr_dt_ps_sensor_stup.attr,
	&dev_attr_dt_stop_auto_motion.attr,
	&dev_attr_airpress_info.attr,
	&dev_attr_airpress_enable.attr,
	&dev_attr_airpress_setdelay.attr,
	&dev_attr_airpress_read_data.attr,
	&dev_attr_airpress_set_calidata.attr,
	&dev_attr_read_airpress.attr,
	&dev_attr_read_temperature.attr,
	&dev_attr_dt_stepcounter_stup.attr,
	&dev_attr_handpress_calibrate.attr,
	&dev_attr_handpress_selfTest.attr,
	&dev_attr_handpress_info.attr,
	&dev_attr_handpress_enable.attr,
	&dev_attr_handpress_setdelay.attr,
	&dev_attr_handpress_read_data.attr,
	&dev_attr_ois_ctrl.attr,
	&dev_attr_iom3_sr_status.attr,
	&dev_attr_dump_sensor_status.attr,
	&dev_attr_cap_prox_calibrate.attr,
	&dev_attr_cap_prox_freespace.attr,
	&dev_attr_cap_prox_enable.attr,
	&dev_attr_cap_prox_setdelay.attr,
	&dev_attr_key_debug.attr,
	&dev_attr_sar_data.attr,
	&dev_attr_magn_bracket_enable.attr,
	&dev_attr_magn_bracket_setdelay.attr,
	&dev_attr_hifi_supported.attr,
	&dev_attr_sensor_detect.attr,
	NULL
};

static const struct attribute_group sensor_node = {
	.attrs = sensor_attributes,
};

static struct platform_device sensor_input_info = {
	.name = "huawei_sensor",
	.id = -1,
};

static int __init sensor_input_info_init(void)
{
	int ret;

	if (is_sensorhub_disabled())
		return -1;
	spin_lock_init(get_fsdata_lock());
	ret = platform_device_register(&sensor_input_info);
	if (ret) {
		hwlog_err("%s: platform_device_register failed, ret:%d.\n", __func__, ret);
		goto REGISTER_ERR;
	}

	ret = sysfs_create_group(&sensor_input_info.dev.kobj, &sensor_node);
	if (ret) {
		hwlog_err("%s sysfs_create_group error ret =%d.\n", __func__, ret);
		goto SYSFS_CREATE_CGOUP_ERR;
	}
	return 0;
SYSFS_CREATE_CGOUP_ERR:
	platform_device_unregister(&sensor_input_info);
REGISTER_ERR:
	return ret;
}

late_initcall_sync(sensor_input_info_init);
MODULE_DESCRIPTION("sensor input info");
MODULE_AUTHOR("huawei driver group of K3V3");
MODULE_LICENSE("GPL");
