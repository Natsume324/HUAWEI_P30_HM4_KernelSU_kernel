#include "als_para_table_hx_hx32062se.h"
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
static hx32062se_als_para_table hx32062se_als_para_diff_tp_color_table[] = {
	{FGD, V4, TS_PANEL_BOE0, 0,
		{1, 1, 14000, 0.454, 3000, 0.615, 0.12, 15277, 10000, 0, 1605, 10000, 1}
	},
	{FGD, V4, TS_PANEL_BOE1, 0,
		{1, 1, 16000, 0.4026, 2000, 0.604, 0.1137, 17164, 10000, 0, 1578, 10000, 2}
	},
	{FGD, V4, TS_PANEL_HUAJIACAI, 0,
		{1, 1, 16000, 0.4026, 2000, 0.604, 0.1137, 17164, 10000, 0, 1578, 10000, 3}
	},
};
hx32062se_als_para_table *als_get_hx32062se_table_by_id(uint32_t id)
{
	if (id >= ARRAY_SIZE(hx32062se_als_para_diff_tp_color_table))
		return NULL;
	return &(hx32062se_als_para_diff_tp_color_table[id]);
}
hx32062se_als_para_table *als_get_hx32062se_first_table(void)
{
	return &(hx32062se_als_para_diff_tp_color_table[0]);
}
uint32_t als_get_hx32062se_table_count(void)
{
	return ARRAY_SIZE(hx32062se_als_para_diff_tp_color_table);
}
