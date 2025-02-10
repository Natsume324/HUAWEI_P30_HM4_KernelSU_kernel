#ifndef __PS_PARA_TABLE_SIP_SIP3510_H__
#define __PS_PARA_TABLE_SIP_SIP3510_H__
#include "sensor_config.h"
enum {
	SIP3510_PS_PARA_PPULSES_IDX = 0,
	SIP3510_PS_PARA_BINSRCH_TARGET_IDX,
	SIP3510_PS_PARA_THRESHOLD_L_IDX,
	SIP3510_PS_PARA_THRESHOLD_H_IDX,
	SIP3510_PS_PARA_BUTT,
};
sip3510_ps_para_table *ps_get_sip3510_table_by_id(uint32_t id);
sip3510_ps_para_table *ps_get_sip3510_first_table(void);
uint32_t ps_get_sip3510_table_count(void);
#endif
