/*
 * Huawei Touchscreen Driver
 *
 * Copyright (c) 2012-2020 Ilitek.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 */

#include "huawei_thp.h"
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/spi/spi.h>
#include <linux/delay.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/of_gpio.h>
#include <linux/gpio.h>

#define ILITEK_IC_NAME "ilitek"
#define THP_ILITEK_DEV_NODE_NAME "ilitek"

#define TDDI_PID_ADDR 0x4009C
#define ILI9881_CHIP 0x9881
#define ILI9882_CHIP 0x9882
#define ENABLE 1
#define DISABLE 0

#define SPI_WRITE 0x82
#define SPI_READ 0x83
#define SPI_ACK  0xA3

#define FUNC_CTRL_CMD_LEN       0x03
#define FUNC_CTRL_HEADER        0x01
#define FUNC_CTRL_SLEEP         0x02
#define DEEP_SLEEP              0x03
#define FUNC_CTRL_GESTURE       0x0A
#define GESTURE_INFO_MODE       0x02
#define P5_X_THP_CMD_PACKET_ID  0x5F
#define P5_X_READ_DATA_CTRL     0xF6

#define GESTURE_DATA_LEN 6
#define GESTURE_PACKET 3
#define GESTURE_RETRY_TIMES 20
#define GESTURE_EVENT 4
#define WAIT_FOR_SPI_BUS_READ_DELAY 10
#define GESTURE_PACKET_ID   0xAA
#define GESTURE_AODCLICK 0x59

#define CMD_HEADER_LEN       1
#define CMD_SHIFT_LEN        3
#define CMD_RETRY_TIMES      3
#define CMD_DEF_RLEN         3
#define CMD_CMD_RETRY_TIMES  2
#define CMD_DELAY_TIME       2
#define CMD_DUMMY_LEN        9
#define CMD_TEMP_LEN         2
#define CMD_BUFFER_LEN       10

#define ENABLE_ICE_MODE_RETRY 3
#define DUMMY_LEN 1
#define ICE_MODE_MAX_LEN 16

#define REGISTER_HEAD 0x25
#define REGISTER_LEN 4
#define TXBUF_LEN 4
#define OFFSET_8 8
#define OFFSET_16 16
#define OFFSET_24 24
#define ICE_MODE_CTRL_CMD 0x181062
#define ICE_MODE_ENABLE_CMD_HEADER_MCU_ON 0x1F
#define ICE_MODE_ENABLE_CMD_HEADER_MCU_OFF 0x25
#define ICE_MODE_DISABLE_CMD_HEADER 0x1B

#define ENABLE_LOWPOWER 0x86
#define DISABLE_LOWPOWER 0x87

#define IC_DETECT_ADDR 0x51024
#define IC_DETECT_DATA 0xA55A5AA5

static u32 g_chip_id;

/* dectect IC by dummy reg */
struct ic_feature_config {
	unsigned int detect_by_dummy_reg;
	unsigned int deepsleepin_support;
};

static struct ic_feature_config ic_config;
static struct udfp_mode_status ud_mode_status;

static int touch_driver_spi_read_write(struct spi_device *client,
	void *tx_buf, void *rx_buf, size_t len)
{
	struct spi_transfer xfer = {
		.tx_buf = tx_buf,
		.rx_buf = rx_buf,
		.len = len,
	};
	struct spi_message msg;
	int ret;

	spi_message_init(&msg);
	spi_message_add_tail(&xfer, &msg);

	ret = thp_bus_lock();
	if (ret < 0) {
		thp_log_err("%s:get lock failed\n", __func__);
		return -EINVAL;
	}

	ret = spi_sync(client, &msg);
	if (ret < 0) {
		thp_log_err("%s:spi_sync failed\n", __func__);
		thp_bus_unlock();
		return -EINVAL;
	}
	thp_bus_unlock();

	return ret;
}

static void touch_driver_hw_reset(struct thp_device *tdev)
{
	struct thp_core_data *cd = NULL;

	thp_log_info("%s: called,do hardware reset\n", __func__);
	cd = tdev->thp_core;
#ifndef CONFIG_HUAWEI_THP_MTK
	gpio_set_value(tdev->gpios->rst_gpio, THP_RESET_HIGH);
	mdelay(tdev->timing_config.boot_reset_hi_delay_ms);
	gpio_direction_output(tdev->gpios->rst_gpio, GPIO_LOW);
	mdelay(tdev->timing_config.boot_reset_low_delay_ms);
	gpio_set_value(tdev->gpios->rst_gpio, THP_RESET_HIGH);
	mdelay(tdev->timing_config.boot_reset_hi_delay_ms);
#else
	if (cd->support_pinctrl == 0) {
		thp_log_info("%s: not support pinctrl\n", __func__);
		return;
	}
	pinctrl_select_state(cd->pctrl, cd->mtk_pinctrl.reset_high);
	mdelay(tdev->timing_config.boot_reset_hi_delay_ms);
	pinctrl_select_state(cd->pctrl, cd->mtk_pinctrl.reset_low);
	mdelay(tdev->timing_config.boot_reset_low_delay_ms);
	pinctrl_select_state(cd->pctrl, cd->mtk_pinctrl.reset_high);
	mdelay(tdev->timing_config.boot_reset_hi_delay_ms);
#endif
}

static int thp_parse_ic_feature_config(struct device_node *thp_node)
{
	if (of_property_read_u32(thp_node, "detect_by_dummy_reg",
		&ic_config.detect_by_dummy_reg)) {
		thp_log_info("%s: detect_by_dummy_reg, use default 0\n", __func__);
		ic_config.detect_by_dummy_reg = 0;
	}
	thp_log_info("%s: detect_by_dummy_reg = %u\n", __func__, ic_config.detect_by_dummy_reg);

	if (of_property_read_u32(thp_node, "deepsleepin_support",
		&ic_config.deepsleepin_support)) {
		thp_log_info("%s: deepsleepin_support, use default 0\n", __func__);
		ic_config.deepsleepin_support = 0;
	}
	thp_log_info("%s: deepsleepin_support = %u\n", __func__, ic_config.deepsleepin_support);

	return NO_ERR;
}

static int touch_driver_init(struct thp_device *tdev)
{
	int ret;
	struct thp_core_data *cd = tdev->thp_core;
	struct device_node *ilitek_node = of_get_child_by_name(cd->thp_node,
		THP_ILITEK_DEV_NODE_NAME);

	thp_log_info("%s: called\n", __func__);

	if (!ilitek_node) {
		thp_log_err("%s: dev not config in dts\n", __func__);
		return -ENODEV;
	}

	ret = thp_parse_spi_config(ilitek_node, cd);
	if (ret)
		thp_log_err("%s: spi config parse fail\n", __func__);

	ret = thp_parse_timing_config(ilitek_node, &tdev->timing_config);
	if (ret)
		thp_log_err("%s: timing config parse fail\n", __func__);

	ret = thp_parse_feature_config(ilitek_node, cd);
	if (ret)
		thp_log_err("%s: feature_config fail\n", __func__);

	ret = thp_parse_trigger_config(ilitek_node, cd);
	if (ret)
		thp_log_err("%s: trigger_config fail\n", __func__);

	ret = thp_parse_ic_feature_config(ilitek_node);
	if (ret)
		thp_log_err("%s: ic_config parse fail\n", __func__);

	return 0;
}

static int touch_driver_spi_write(struct spi_device *sdev,
	const void *buf, int len)
{
	int ret = 0;
	u8 *txbuf = NULL;
	int safe_size = len + DUMMY_LEN;

	if ((len <= 0) || (len > THP_MAX_FRAME_SIZE)) {
		thp_log_err("%s: spi write len is invaild\n", __func__);
		return -EINVAL;
	}

	txbuf = kzalloc(safe_size, GFP_ATOMIC);
	if (!txbuf) {
		thp_log_err("%s: failed to allocate txbuf\n", __func__);
		ret = -ENOMEM;
		goto out;
	}

	txbuf[0] = SPI_WRITE;
	memcpy(txbuf + DUMMY_LEN, buf, len);

	if (touch_driver_spi_read_write(sdev, txbuf, NULL, safe_size) < 0) {
		thp_log_err("%s: spi write data err in ice mode\n", __func__);
		ret = -EIO;
	}
	kfree(txbuf);
out:
	txbuf = NULL;
	return ret;
}

static int touch_driver_spi_read(struct spi_device *sdev, u8 *buf, int len)
{
	int ret = 0;
	u8 *txbuf = NULL;
	struct thp_core_data *cd = thp_get_core_data();

	if (!buf) {
		thp_log_info("%s: input buf null\n", __func__);
		return -EINVAL;
	}

	txbuf = cd->thp_dev->tx_buff;
	if ((len <= 0) || (len > THP_MAX_FRAME_SIZE)) {
		thp_log_err("%s: spi read len is invaild\n", __func__);
		return -EINVAL;
	}

	txbuf[0] = SPI_READ;
	if (touch_driver_spi_read_write(sdev, txbuf, buf, len) < 0) {
		thp_log_err("%s: spi read data error in ice mode\n", __func__);
		ret = -EIO;
		return ret;
	}

	return ret;
}

static int touch_driver_ice_mode_read(struct spi_device *sdev,
	u32 addr, u32 *data, int len)
{
	int ret;
	int index = 0;
	u8 *rxbuf = NULL;
	u8 txbuf[TXBUF_LEN] = { 0 };

	txbuf[index++] = (u8)REGISTER_HEAD;
	txbuf[index++] = (u8)(addr);
	txbuf[index++] = (u8)(addr >> OFFSET_8);
	txbuf[index++] = (u8)(addr >> OFFSET_16);

	if ((len <= 0) || (len > ICE_MODE_MAX_LEN)) {
		thp_log_err("%s: ice mode read len is invaild\n", __func__);
		return -EINVAL;
	}

	rxbuf = kzalloc(len + DUMMY_LEN, GFP_ATOMIC);
	if (!rxbuf) {
		thp_log_err("%s: Failed to allocate rxbuf\n", __func__);
		return -ENOMEM;
	}

	ret = touch_driver_spi_write(sdev, txbuf, REGISTER_LEN);
	if (ret < 0) {
		thp_log_err("%s: spi write failed\n", __func__);
		goto out;
	}

	ret = touch_driver_spi_read(sdev, rxbuf, len + DUMMY_LEN);
	if (ret < 0) {
		thp_log_err("%s: spi read failed\n", __func__);
		goto out;
	}

	if (len == sizeof(u8))
		*data = rxbuf[1];
	else
		*data = ((rxbuf[1]) | (rxbuf[2] << OFFSET_8) |
			(rxbuf[3] << OFFSET_16) | (rxbuf[4] << OFFSET_24));

out:
	if (ret < 0)
		thp_log_err("%s: failed to read data in ice mode, ret = %d\n",
			__func__, ret);

	kfree(rxbuf);
	rxbuf = NULL;
	return ret;
}

static int touch_driver_ice_mode_write(struct spi_device *sdev, u32 addr, u32 data, int len)
{
	int ret = 0;
	int i = 0;
	u8 txbuf[ICE_MODE_MAX_LEN] = { 0 };

	if ((len <= 0) || (len > (ICE_MODE_MAX_LEN - REGISTER_LEN))) {
		thp_log_err("%s: ice mode write len = %d is invaild\n", __func__, len);
		return -EINVAL;
	}

	txbuf[0] = REGISTER_HEAD;
	txbuf[1] = (u8)(addr);
	txbuf[2] = (u8)(addr >> OFFSET_8);
	txbuf[3] = (u8)(addr >> OFFSET_16);
	for (i = 0; i < len; i++)
		txbuf[i + REGISTER_LEN] = (u8)(data >> (OFFSET_8 * i));

	ret = touch_driver_spi_write(sdev, txbuf, len + REGISTER_LEN);
	if (ret < 0)
		thp_log_err("Failed to read data in ice mode, ret = %d\n", ret);

	return ret;
}

static int touch_driver_detect_by_dummy(struct thp_device *tdev)
{
	u32 wdata = IC_DETECT_DATA;
	u32 rdata = 0;
	int detect_result = -1;

	if (touch_driver_ice_mode_write(tdev->thp_core->sdev, IC_DETECT_ADDR,
		wdata, sizeof(u32)) < 0)
		thp_log_err("Write dummy error\n");

	if (touch_driver_ice_mode_read(tdev->thp_core->sdev, IC_DETECT_ADDR,
		&rdata, sizeof(u32)) < 0)
		thp_log_err("Read dummy error\n");

	thp_log_info("%s: rdata 0x%x\n", __func__, rdata);
	if (rdata == wdata)
		detect_result = 0;

	return detect_result;
}

static int touch_driver_ice_mode_ctrl(struct thp_device *tdev,
	bool ice_enable, bool mcu_enable)
{
	int i;
	int ret = 0;
	int index = 0;
	u32 chip_id = 0;
	u8 cmd[REGISTER_LEN] = {0};

	thp_log_info("%s: ice_enable = %d, mcu_enable = %d\n", __func__,
		ice_enable, mcu_enable);

	cmd[index++] = (u8)ICE_MODE_ENABLE_CMD_HEADER_MCU_OFF;
	cmd[index++] = (u8)(ICE_MODE_CTRL_CMD);
	cmd[index++] = (u8)(ICE_MODE_CTRL_CMD >> OFFSET_8);
	cmd[index++] = (u8)(ICE_MODE_CTRL_CMD >> OFFSET_16);

	if (ice_enable) {
		if (mcu_enable)
			cmd[0] = ICE_MODE_ENABLE_CMD_HEADER_MCU_ON;
		for (i = 0; i < ENABLE_ICE_MODE_RETRY; i++) {
			touch_driver_hw_reset(tdev);
			if (touch_driver_spi_write(tdev->thp_core->sdev, cmd,
				sizeof(cmd)) < 0)
				thp_log_err("%s: write ice mode cmd error\n",
					__func__);

			if (touch_driver_ice_mode_read(tdev->thp_core->sdev,
				TDDI_PID_ADDR, &chip_id, sizeof(u32)) < 0)
				thp_log_err("%s: Read chip_pid error\n",
					__func__);

			thp_log_info("%s: chipid 0x%X\n", __func__, chip_id);
			if (ic_config.detect_by_dummy_reg) {
				if (!touch_driver_detect_by_dummy(tdev))
					break;
			} else {
				if (((chip_id >> OFFSET_16) == ILI9881_CHIP) ||
					((chip_id >> OFFSET_16) == ILI9882_CHIP))
					break;
			}
		}

		if (i >= ENABLE_ICE_MODE_RETRY) {
			thp_log_err("%s: Enter to ICE Mode failed\n", __func__);
			return -EINVAL;
		}
		g_chip_id = chip_id;
	} else {
		cmd[0] = ICE_MODE_DISABLE_CMD_HEADER;
		ret = touch_driver_spi_write(tdev->thp_core->sdev,
			cmd, sizeof(cmd));
		if (ret < 0)
			thp_log_err("%s: Exit to ICE Mode failed\n", __func__);
	}

	return ret;
}

static u8 touch_driver_calc_packet_checksum(u8 *packet, int len)
{
	int i;
	s32 sum = 0;

	if (!packet) {
		thp_log_err("%s: packet is null\n", __func__);
		return 0;
	}

	for (i = 0; i < len; i++)
		sum += packet[i];

	return (u8) ((-sum) & 0xFF);
}

static int touch_driver_send_cmd_write(struct spi_device *sdev, const u8* cmd, u32 w_len)
{
	u8 dummy_data[CMD_DUMMY_LEN];
	u8 temp[CMD_TEMP_LEN];

	// 0. write dummy cmd
	memset(dummy_data, SPI_ACK, sizeof(dummy_data));
	if (touch_driver_spi_write(sdev, dummy_data, sizeof(dummy_data)) < 0) {
		thp_log_err("%s: write dummy cmd fail\n", __func__);
		return -EINVAL;
	}
	mdelay(CMD_DELAY_TIME);
	// 1. 0xf6 wlen
	temp[0] = P5_X_READ_DATA_CTRL;
	temp[1] = w_len;
	if (touch_driver_spi_write(sdev, temp, sizeof(temp)) < 0) {
		thp_log_err("%s: write pre cmd fail\n", __func__);
		return -EINVAL;
	}
	mdelay(CMD_DELAY_TIME);
	// 2. write cmd
	if (touch_driver_spi_write(sdev, cmd, w_len) < 0) {
		thp_log_err("%s: write CMD fail\n", __func__);
		return -EINVAL;
	}
	mdelay(CMD_DELAY_TIME);

	return 0;
}

static int touch_driver_check_cmd(struct spi_device *sdev, u8 *cmd_buffer, int thp_rlen)
{
	u8 checksum;
	int shift = CMD_SHIFT_LEN;
	int retry = CMD_RETRY_TIMES;
	int ret = 0;

	while (retry--) {
		ret = touch_driver_spi_read(sdev, cmd_buffer, thp_rlen);
		if (cmd_buffer[DUMMY_LEN] != SPI_ACK) {
			thp_log_err("%s: ack 0x%x error\n", __func__, cmd_buffer[DUMMY_LEN]);
			return ret;
		}
		if (cmd_buffer[shift] == P5_X_THP_CMD_PACKET_ID) {
			// check checksum
			checksum = touch_driver_calc_packet_checksum(&cmd_buffer[shift],
				thp_rlen - sizeof(checksum) - shift);
			if (checksum == cmd_buffer[thp_rlen - sizeof(checksum)])
				break;
			else
				thp_log_err("%s: checksum or header wrong, checksum = %x buf = %x\n",
					__func__, checksum, cmd_buffer[thp_rlen-1]);
		}
		mdelay(CMD_DELAY_TIME);
	}

	return ret;
}

static int touch_driver_send_cmd(struct spi_device *sdev, const u8* cmd,
	u32 w_len)
{
	u8 cmd_buffer[CMD_BUFFER_LEN];
	int thp_rlen;
	int ret = 0;
	int header = CMD_HEADER_LEN;
	int shift = CMD_SHIFT_LEN;
	int cmd_retry = CMD_CMD_RETRY_TIMES;

	thp_rlen = CMD_DEF_RLEN + header + shift;
	thp_log_info("%s: cmd = 0x%X, write len = %d read len = %d\n",
		__func__, cmd[0], w_len, thp_rlen);

	while (cmd_retry--) {
		memset(cmd_buffer, 0, CMD_BUFFER_LEN);
		if (touch_driver_send_cmd_write(sdev, cmd, w_len))
			return -EINVAL;
		ret = touch_driver_check_cmd(sdev, cmd_buffer, thp_rlen);
		if (!ret) {
			thp_log_info("%s: send thp cmd ok\n", __func__);
			break;
		} else {
			thp_log_err("%s: send fail cmd = 0x%X buf[0] = 0x%X buf[1] = 0x%X buf[shift] = 0x%X\n",
				__func__, cmd[0], cmd_buffer[0], cmd_buffer[1], cmd_buffer[shift]);
		}
	}

	return ret;
}

static int send_deepsleep_cmd_to_ic(void)
{
	struct thp_core_data *cd = thp_get_core_data();
	int ret = 0;
	u8 cmd_deepsleepin[FUNC_CTRL_CMD_LEN] = {
		FUNC_CTRL_HEADER, FUNC_CTRL_SLEEP, DEEP_SLEEP };

	if (ic_config.deepsleepin_support)
		ret = touch_driver_send_cmd(cd->sdev,
			cmd_deepsleepin, FUNC_CTRL_CMD_LEN);

	return ret;
}

static int touch_driver_chip_detect(struct thp_device *tdev)
{
	int ret;

	ret = touch_driver_ice_mode_ctrl(tdev, ENABLE, DISABLE);
	/* pull reset to exit ice mode */
	touch_driver_hw_reset(tdev);
	if (ret) {
		thp_log_err("%s: chip is not detected\n", __func__);
		return -ENODEV;
	}

	return 0;
}

static int touch_driver_get_frame(struct thp_device *tdev,
	char *buf, unsigned int len)
{
	int ret;

	if (!tdev) {
		thp_log_err("%s: input dev null\n", __func__);
		return -ENOMEM;
	}

	ret = touch_driver_spi_read(tdev->thp_core->sdev, buf, len);
	if (ret < 0) {
		thp_log_err("%s: Read frame packet failed, ret = %d\n",
			__func__, ret);
		return ret;
	}

	return 0;
}

static int touch_driver_resume(struct thp_device *tdev)
{
	struct thp_core_data *cd = NULL;

	thp_log_info("%s: called\n", __func__);
	cd = tdev->thp_core;
#ifndef CONFIG_HUAWEI_THP_MTK
	gpio_set_value(tdev->gpios->cs_gpio, GPIO_HIGH);
	gpio_set_value(tdev->gpios->rst_gpio, THP_RESET_HIGH);
	if (tdev->thp_core->aod_support_on_tddi)
		ud_mode_status.lowpower_mode = 0;
#else
	if (cd->support_pinctrl == 0) {
		thp_log_info("%s: not support pinctrl\n", __func__);
		return 0;
	}
	pinctrl_select_state(cd->pctrl, cd->pins_default);
#endif
	mdelay(tdev->timing_config.resume_reset_after_delay_ms);

	return 0;
}

static void touch_driver_gesture_mode(struct thp_device *tdev)
{
	struct thp_core_data *cd = NULL;
	u8 cmd_gesture_mode[FUNC_CTRL_CMD_LEN]  = {
		FUNC_CTRL_HEADER, FUNC_CTRL_GESTURE, GESTURE_INFO_MODE };

	cd = tdev->thp_core;
	thp_log_info("%s: gesture mode\n", __func__);
	touch_driver_send_cmd(cd->sdev,
		cmd_gesture_mode, FUNC_CTRL_CMD_LEN);
	mutex_lock(&cd->thp_wrong_touch_lock);
	cd->easy_wakeup_info.off_motion_on = true;
	mutex_unlock(&cd->thp_wrong_touch_lock);
}

static void touch_driver_power_off_mode(struct thp_device *tdev)
{
	thp_log_info("%s:power off mode set rst and cs low\n", __func__);
	gpio_set_value(tdev->gpios->cs_gpio, GPIO_LOW);
	gpio_set_value(tdev->gpios->rst_gpio, THP_RESET_LOW);
}

static int touch_driver_suspend(struct thp_device *tdev)
{
	int pt_test_mode = is_pt_test_mode(tdev);
	unsigned int gesture_status;
	struct thp_core_data *cd = NULL;
	uint8_t ic_sleep_cmd = ENABLE_LOWPOWER;

	cd = tdev->thp_core;
	gesture_status = ((cd->easy_wakeup_info.sleep_mode == TS_GESTURE_MODE)
		&& cd->lcd_gesture_mode_support) ||
		(tdev->thp_core->tddi_aod_screen_off_flag) || (cd->aod_touch_status);
	thp_log_info("%s: called\n", __func__);
	if (pt_test_mode) {
		thp_log_info("%s: This is pt test mode\n", __func__);
		return send_deepsleep_cmd_to_ic();
	} else if (gesture_status) {
		touch_driver_gesture_mode(tdev);
		if (cd->aod_support_on_tddi && cd->tp_ud_lowpower_status) {
			touch_driver_send_cmd(tdev->thp_core->sdev, &ic_sleep_cmd, sizeof(ic_sleep_cmd));
			ud_mode_status.lowpower_mode = cd->tp_ud_lowpower_status;
			thp_log_info("%s: complementary send tp lowpower cmd succ\n", __func__);
		}
	} else {
		thp_log_info("%s: This is normal sleep-in mode\n", __func__);
		if (cd->self_control_power)
			thp_log_info("%s: Self-controlled power-off\n", __func__);
		else
			if (ic_config.deepsleepin_support)
				touch_driver_power_off_mode(tdev);
	}

	return 0;
}

static int parse_event_info(struct thp_device *tdev,
	u8 *buf, struct thp_udfp_data *udfp_data)
{
	int gesture_flag;

	if (!buf) {
		thp_log_info("%s: input buf null\n", __func__);
		return -EINVAL;
	}
	gesture_flag = (buf[GESTURE_PACKET] == GESTURE_PACKET_ID);
	if (gesture_flag) {
		udfp_data->aod_event = buf[GESTURE_EVENT];
		thp_log_info("%s:aod:%u\n", __func__, udfp_data->aod_event);
		if (udfp_data->aod_event == GESTURE_AODCLICK)
			udfp_data->aod_event = AOD_VALID_EVENT;
	} else {
		thp_log_err("[%s] read 0x%X, 0x%X, 0x%X, 0x%X, 0x%X\n",
			__func__, buf[0], buf[1], buf[2], buf[3], buf[4]);
		return -EINVAL;
	}
	return 0;
}

static int touch_driver_get_event_info(struct thp_device *tdev,
	struct thp_udfp_data *udfp_data)
{
	int ret;
	unsigned int i;
	int retry_times = GESTURE_RETRY_TIMES;
	u8 gesture_buffer[GESTURE_DATA_LEN] = {0};

	thp_log_info("%s: called\n", __func__);
	for (i = 0; i < retry_times; i++) {
		ret = touch_driver_spi_read(tdev->thp_core->sdev,
			gesture_buffer, GESTURE_DATA_LEN);
		if (ret == 0)
			break;
		thp_log_info("%s: spi write abnormal ret %d retry\n", __func__, ret);
		msleep(WAIT_FOR_SPI_BUS_READ_DELAY); /* retry time delay */
	}
	if (ret < 0) {
		thp_log_err("Read gesture packet failed, ret = %d\n", ret);
		return ret;
	}

	ret = parse_event_info(tdev, gesture_buffer, udfp_data);
	if (ret < 0) {
		thp_log_err("%s: Failed to check gesture data\n", __func__);
		return ret;
	}

	return 0;
}

static void touch_driver_exit(struct thp_device *tdev)
{
	thp_log_info("%s: called\n", __func__);
	kfree(tdev->tx_buff);
	tdev->tx_buff = NULL;
	kfree(tdev);
	tdev = NULL;
}

static int touch_driver_set_lowpower_state(struct thp_device *tdev,
	u8 state)
{
	uint8_t cmd;
	int ret = 0;
	struct thp_core_data *cd = NULL;

	cd = thp_get_core_data();
	thp_log_info("%s: called state = %u\n", __func__, state);
	if (tdev == NULL) {
		thp_log_err("%s: tdev null\n", __func__);
		return -EINVAL;
	}
	if (!cd->aod_state_flag && cd->work_status != SUSPEND_DONE) {
		thp_log_info("%s: resumed, not handle lp\n", __func__);
		return NO_ERR;
	}
	thp_log_info("%s: ud_mode_status.lowpower_mode = %u\n", __func__, ud_mode_status.lowpower_mode);
	if (ud_mode_status.lowpower_mode == state) {
		thp_log_info("%s:don't repeat old status %u\n", __func__, state);
		return 0;
	}

	if (state)
		cmd = ENABLE_LOWPOWER; /* enable lowpower */
	else
		cmd = DISABLE_LOWPOWER; /* disable lowpower */
	ret = touch_driver_send_cmd(tdev->thp_core->sdev, &cmd, sizeof(cmd));
	ud_mode_status.lowpower_mode = state;
	return ret;
}

struct thp_device_ops ilitek_dev_ops = {
	.init = touch_driver_init,
	.detect = touch_driver_chip_detect,
	.get_frame = touch_driver_get_frame,
	.resume = touch_driver_resume,
	.suspend = touch_driver_suspend,
	.exit = touch_driver_exit,
	.get_event_info = touch_driver_get_event_info,
	.tp_lowpower_ctrl = touch_driver_set_lowpower_state,
};

static int __init touch_driver_module_init(void)
{
	int ret;
	struct thp_device *dev = NULL;
	struct thp_core_data *cd = thp_get_core_data();

	thp_log_info("%s: called\n", __func__);
	dev = kzalloc(sizeof(*dev), GFP_KERNEL);
	if (!dev) {
		thp_log_err("%s: thp device malloc fail\n", __func__);
		return -ENOMEM;
	}

	dev->tx_buff = kzalloc(THP_MAX_FRAME_SIZE, GFP_KERNEL);
	if (!dev->tx_buff) {
		thp_log_err("%s: out of memory\n", __func__);
		ret = -ENOMEM;
		goto err;
	}

	dev->ic_name = ILITEK_IC_NAME;
	dev->ops = &ilitek_dev_ops;
	if (cd && cd->fast_booting_solution) {
		thp_send_detect_cmd(dev, NO_SYNC_TIMEOUT);
		/*
		 * The thp_register_dev will be called later to complete
		 * the real detect action.If it fails, the detect function will
		 * release the resources requested here.
		 */
		return 0;
	}

	ret = thp_register_dev(dev);
	if (ret) {
		thp_log_err("%s: register fail\n", __func__);
		goto err;
	}

	return ret;
err:
	kfree(dev->tx_buff);
	dev->tx_buff = NULL;
	kfree(dev);
	dev = NULL;
	return ret;
}

static void __exit touch_driver_module_exit(void)
{
	thp_log_info("%s: called\n", __func__);
};

module_init(touch_driver_module_init);
module_exit(touch_driver_module_exit);
MODULE_AUTHOR("ilitek");
MODULE_DESCRIPTION("ilitek driver");
MODULE_LICENSE("GPL");
