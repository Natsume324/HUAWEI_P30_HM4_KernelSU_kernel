/*
 * lcdkit_fb_util.h
 *
 * lcdkit fb util head file for lcd driver
 *
 * Copyright (c) 2018-2020 Huawei Technologies Co., Ltd.
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

#ifndef __LCDKIT_FB_UTIL_H_
#define __LCDKIT_FB_UTIL_H_

#include "lcdkit_panel.h"

int lcdkit_fb_create_sysfs(struct kobject* obj);
void lcdkit_fb_remove_sysfs(struct kobject* obj);
ssize_t lcd_cabc_mode_store(struct device* dev, struct lcdkit_panel_data* lcdkit_info, const char* buf);
ssize_t lcd_inversion_mode_store(struct device* dev, struct lcdkit_panel_data* lcdkit_info, const char* buf);
ssize_t lcd_scan_mode_store(struct device* dev, struct lcdkit_panel_data* lcdkit_info, const char* buf);
ssize_t lcd_check_reg_show(struct device* dev, struct lcdkit_panel_data* lcdkit_info, char* buf);
ssize_t lcd_gram_check_show(struct device* dev, struct lcdkit_panel_data* lcdkit_info, char* buf);
ssize_t lcd_gram_check_store(struct device* dev, struct lcdkit_panel_data* lcdkit_info, const char* buf);
ssize_t lcd_dynamic_sram_check_show(struct device* dev, struct lcdkit_panel_data* lcdkit_info, char* buf);
ssize_t lcd_dynamic_sram_check_store(struct device* dev, struct lcdkit_panel_data* lcdkit_info, const char* buf);
ssize_t lcd_ic_color_enhancement_mode_show(struct device* dev, struct lcdkit_panel_data* lcdkit_info, char* buf);
ssize_t lcd_ic_color_enhancement_mode_store(struct device* dev, struct lcdkit_panel_data* lcdkit_info, const char* buf);
ssize_t lcd_sleep_ctrl_store(struct device* dev, struct lcdkit_panel_data* lcdkit_info, const char* buf);
ssize_t lcd_lp2hs_mipi_check_show(struct device* dev, struct lcdkit_panel_data* lcdkit_info, char* buf);
ssize_t lcd_lp2hs_mipi_check_store(struct device* dev, struct lcdkit_panel_data* lcdkit_info, const char* buf);
ssize_t lcd_bist_check(struct device* dev, struct lcdkit_panel_data* lcdkit_info, char* buf);
ssize_t lcd_mipi_detect_show(struct device* dev, struct lcdkit_panel_data* lcdkit_info, char* buf);
ssize_t lcd_voltage_mode_enable_store(struct device* dev, struct lcdkit_panel_data* lcdkit_info, const char* buf);
ssize_t lcd_acl_ctrl_show(struct device* dev, struct lcdkit_panel_data* lcdkit_info, char* buf);
ssize_t lcd_acl_ctrl_store(struct device* dev, struct lcdkit_panel_data* lcdkit_info, const char* buf);
ssize_t lcd_amoled_vr_mode_show(struct device* dev, struct lcdkit_panel_data* lcdkit_info, char* buf);
ssize_t lcd_amoled_vr_mode_store(struct device* dev, struct lcdkit_panel_data* lcdkit_info, const char* buf);
ssize_t lcd_hbm_ctrl_store(struct device* dev, struct lcdkit_panel_data* lcdkit_info, const char* buf);
ssize_t lcd_support_mode_show(struct device* dev, struct lcdkit_panel_data* lcdkit_info, char* buf);
ssize_t lcd_support_mode_store(struct device* dev, struct lcdkit_panel_data* lcdkit_info, const char* buf);
ssize_t lcd_comform_mode_show(struct device* dev, struct device_attribute* attr, char* buf);
ssize_t lcd_comform_mode_store(struct device* dev, struct device_attribute* attr, const char* buf, size_t count);
ssize_t lcd_cinema_mode_show(struct device* dev, struct device_attribute* attr, char* buf);
ssize_t lcd_cinema_mode_store(struct device* dev, struct device_attribute* attr, const char* buf, size_t count);
ssize_t lcd_support_checkmode_show(struct device* dev, struct lcdkit_panel_data* lcdkit_info, char* buf);
ssize_t led_rg_lcd_color_temperature_show(struct device* dev, struct device_attribute* attr, char* buf);
ssize_t led_rg_lcd_color_temperature_store(struct device* dev, struct device_attribute* attr, const char* buf, size_t count);
ssize_t lcd_ce_mode_show(struct device* dev, struct device_attribute* attr, char* buf);
ssize_t lcd_ce_mode_store(struct device* dev, struct device_attribute* attr, const char* buf, size_t count);
ssize_t effect_al_show(struct device* dev, struct device_attribute* attr, char* buf);
ssize_t effect_al_store(struct device* dev, struct device_attribute* attr, const char* buf, size_t count);
ssize_t effect_ce_show(struct device* dev, struct device_attribute* attr, char* buf);
ssize_t effect_ce_store(struct device* dev, struct device_attribute* attr, const char* buf, size_t count);
ssize_t lcd_effect_sre_show(struct device *dev, struct device_attribute *attr, char *buf);
ssize_t lcd_effect_sre_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count);
ssize_t effect_bl_show(struct device* dev, struct device_attribute* attr, char* buf);
ssize_t effect_bl_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count);
ssize_t effect_bl_enable_show(struct device* dev, struct device_attribute* attr, char* buf);
ssize_t effect_bl_enable_store(struct device* dev, struct device_attribute* attr, const char* buf, size_t count);
ssize_t effect_metadata_show(struct device* dev, struct device_attribute* attr, char* buf);
ssize_t effect_metadata_store(struct device* dev, struct device_attribute* attr, const char* buf, size_t count);
ssize_t effect_available_show(struct device* dev, struct device_attribute* attr, char* buf);
ssize_t gamma_dynamic_store(struct device* dev,	struct device_attribute* attr, const char* buf, size_t count);
ssize_t  lcd_2d_sharpness_show(struct device* dev, struct device_attribute* attr, char* buf);
ssize_t lcd_2d_sharpness_store(struct device* dev,  struct device_attribute* attr, const char* buf, size_t count);
ssize_t lcd_acm_state_show(struct device* dev, struct device_attribute* attr, char* buf);
ssize_t lcd_acm_state_store(struct device* dev, struct device_attribute* attr, const char* buf, size_t count);
ssize_t lcd_gmp_state_show(struct device* dev, struct device_attribute* attr, char* buf);
ssize_t lcd_gmp_state_store(struct device* dev, struct device_attribute* attr, const char* buf, size_t count);
ssize_t sbl_ctrl_show(struct device* dev,	struct device_attribute* attr, char* buf);
ssize_t sbl_ctrl_store(struct device* dev,	struct device_attribute* attr, const char* buf, size_t count);
ssize_t lcd_color_temperature_show(struct device* dev, struct device_attribute* attr, char* buf);
ssize_t lcd_color_temperature_store(struct device* dev, struct device_attribute* attr, const char* buf, size_t count);
ssize_t lcd_frame_count_show(struct device* dev, struct device_attribute* attr, const char* buf);
ssize_t lcd_frame_update_show(struct device* dev,	struct device_attribute* attr, char* buf);
ssize_t lcd_frame_update_store(struct device* dev, struct device_attribute* attr, const char* buf, size_t count);
ssize_t mipi_dsi_bit_clk_upt_show(struct device* dev, struct device_attribute* attr, char* buf);
ssize_t mipi_dsi_bit_clk_upt_store(struct device* dev, struct device_attribute* attr, const char* buf, size_t count);
ssize_t lcd_fps_scence_show(struct device* dev, struct device_attribute* attr, char* buf);
ssize_t lcd_fps_scence_store(struct device* dev, struct device_attribute* attr, const char* buf, size_t count);
ssize_t alpm_function_show(struct device* dev,	struct device_attribute* attr, char* buf);
ssize_t alpm_function_store(struct device* dev, struct device_attribute* attr, const char* buf);
ssize_t alpm_setting_show(struct device* dev, struct device_attribute* attr, char* buf);
ssize_t alpm_setting_store(struct device* dev, struct device_attribute* attr, const char* buf, size_t count);
ssize_t lcd_func_switch_show(struct device* dev, struct device_attribute* attr, char* buf);
ssize_t lcd_func_switch_store(struct device* dev, struct device_attribute* attr, const char* buf, size_t count);
ssize_t lcd_dynamic_porch_show(struct device* dev, struct device_attribute* attr, char* buf);
ssize_t lcd_dynamic_porch_store(struct device* dev, struct device_attribute* attr, const char* buf, size_t count);
ssize_t lcd_test_config_show(struct device* dev, struct lcdkit_panel_data* lcdkit_info, char* buf);
ssize_t lcd_test_config_store(struct device* dev, struct lcdkit_panel_data* lcdkit_info, const char* buf);
ssize_t lv_detect_show(struct device* dev, struct lcdkit_panel_data* lcdkit_info, char* buf);
ssize_t current_detect_show(struct device* dev, struct lcdkit_panel_data* lcdkit_info, char* buf);
ssize_t lcd_reg_read_show(struct device* dev, struct lcdkit_panel_data* lcdkit_info, char* buf);
ssize_t lcd_reg_read_store(struct device* dev, struct lcdkit_panel_data* lcdkit_info, const char* buf);
ssize_t get_lcdkit_support(void);
ssize_t lcd_ddic_oem_info_show(struct device* dev, struct lcdkit_panel_data* lcdkit_inf, char* buf);
ssize_t lcd_ddic_oem_info_store(struct device* dev, struct lcdkit_panel_data* lcdkit_inf, const char* buf);
ssize_t lcd_bl_mode_show(struct device* dev, struct lcdkit_panel_data* lcdkit_info, char* buf);
ssize_t lcd_se_mode_store(struct device* dev, struct lcdkit_panel_data* lcdkit_info, const char* buf);
ssize_t lcd_se_mode_show(struct device* dev, struct lcdkit_panel_data* lcdkit_info, char* buf);
ssize_t lcd_bl_mode_store(struct device* dev, struct lcdkit_panel_data* lcdkit_info, const char* buf);
ssize_t lcd_support_bl_mode_show(struct device* dev, struct lcdkit_panel_data* lcdkit_info, char* buf);
ssize_t lcd_ldo_check_show(struct device* dev, char* buf);
ssize_t lcd_mipi_config_store(struct device* dev, struct lcdkit_panel_data* lcdkit_info, const char* buf);
ssize_t lcd_panel_sncode_show(struct device *dev, char *buf);
ssize_t display_idle_mode_show(struct device* dev, struct device_attribute* attr, char* buf);
ssize_t display_idle_mode_store(struct device* dev, struct device_attribute* attr, const char* buf);
extern int lcdkit_get_vsp_voltage(void);
extern int lcdkit_get_vsn_voltage(void);
#endif
