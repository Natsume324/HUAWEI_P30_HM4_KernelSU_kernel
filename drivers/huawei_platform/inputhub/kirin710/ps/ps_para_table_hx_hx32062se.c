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
#include "ps_para_table_hx_hx32062se.h"
static hx32062se_ps_para_table hx32062se_ps_para_diff_tp_color_table[] = {
	{FGD, V4, TS_PANEL_TIANMA, 0, {103, 172, 4500, 750}},
	{FGD, V4, TS_PANEL_SKYWORTH, 0, {118, 191, 6000, 750}},
};
hx32062se_ps_para_table *ps_get_hx32062se_table_by_id(uint32_t id)
{
	if (id >= ARRAY_SIZE(hx32062se_ps_para_diff_tp_color_table))
		return NULL;
	return &(hx32062se_ps_para_diff_tp_color_table[id]);
}
hx32062se_ps_para_table *ps_get_hx32062se_first_table(void)
{
	return &(hx32062se_ps_para_diff_tp_color_table[0]);
}
uint32_t ps_get_hx32062se_table_count(void)
{
	return ARRAY_SIZE(hx32062se_ps_para_diff_tp_color_table);
}
