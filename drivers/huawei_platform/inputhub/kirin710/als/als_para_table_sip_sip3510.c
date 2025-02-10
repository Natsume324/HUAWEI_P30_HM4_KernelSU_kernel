#include "als_para_table_sip_sip3510.h"
#include <linux/err.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <securec.h>
#include "tp_color.h"
#include "contexthub_boot.h"
#include "contexthub_route.h"
static sip3510_als_para_table sip3510_als_para_diff_tp_color_table[] = {
	{ FGD, V3, TS_PANEL_BOE0,      0, { 1 } },
	{ FGD, V4, TS_PANEL_BOE0,      0, { 2 } },
	{ FGD, V4, TS_PANEL_BOE1,      0, { 3 } },
	{ FGD, V4, TS_PANEL_HUAJIACAI, 0, { 4 } },
};
sip3510_als_para_table *als_get_sip3510_table_by_id(uint32_t id)
{
	if (id >= ARRAY_SIZE(sip3510_als_para_diff_tp_color_table))
		return NULL;
	return &(sip3510_als_para_diff_tp_color_table[id]);
}
sip3510_als_para_table *als_get_sip3510_first_table(void)
{
	return &(sip3510_als_para_diff_tp_color_table[0]);
}
uint32_t als_get_sip3510_table_count(void)
{
	return ARRAY_SIZE(sip3510_als_para_diff_tp_color_table);
}
