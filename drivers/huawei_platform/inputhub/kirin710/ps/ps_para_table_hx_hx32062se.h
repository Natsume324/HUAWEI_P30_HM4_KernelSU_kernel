#ifndef __PS_PARA_TABLE_HX_HX32062SE_H__
#define __PS_PARA_TABLE_HX_HX32062SE_H__
#include "sensor_config.h"
enum {
	HX32062SE_PS_PARA_PPULSES_IDX = 0,
	HX32062SE_PS_PARA_BINSRCH_TARGET_IDX,
	HX32062SE_PS_PARA_THRESHOLD_L_IDX,
	HX32062SE_PS_PARA_THRESHOLD_H_IDX,
	HX32062SE_PS_PARA_BUTT,
};
hx32062se_ps_para_table *ps_get_hx32062se_table_by_id(uint32_t id);
hx32062se_ps_para_table *ps_get_hx32062se_first_table(void);
uint32_t ps_get_hx32062se_table_count(void);
#endif
