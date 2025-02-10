 /*
  * Copyright (c) Huawei Technologies Co., Ltd. 2021-2023. All rights reserved.
  * Description: Control interfaces of some virtual memory features in the general Linux system.
  * Author: Lei Run <leirun1@huawei.com>
  * Create: 2022-9-26
  */

#include <linux/mm.h>
#include <linux/module.h>
#include <linux/sysctl.h>

#ifdef CONFIG_SPLIT_SHRINKER
#include <linux/split_shrinker.h>
#endif

static int zero_int;
static int one_int = 1;
static int one_hundred = 100;

#ifdef CONFIG_SPLIT_SHRINKER
/*
 * Don't shrink delay_shrinkers unless reclaim prio is less than 8
 */
static int g_delay_shrinker_prio = 8;
int get_delay_shrinker_prio(void)
{
	return g_delay_shrinker_prio;
}

#ifdef CONFIG_SPLIT_SHRINKER_DEBUG
static int g_split_shrinker_enable = 1;
bool is_split_shrinker_enable(void)
{
	return g_split_shrinker_enable == 1;
}

static int g_enable_register_skip_shrinker;
bool is_debug_skip_shrinker_enable(void)
{
	return g_enable_register_skip_shrinker == 1;
}
#endif
#endif

static struct ctl_table vm_table[] = {
#ifdef CONFIG_SPLIT_SHRINKER
	{
		.procname	= "delay_shrinker_prio",
		.data		= &g_delay_shrinker_prio,
		.maxlen		= sizeof(g_delay_shrinker_prio),
		.mode		= 0644,
		.proc_handler	= proc_dointvec_minmax,
		.extra1		= &zero_int,
		.extra2		= &one_hundred,
	},
#ifdef CONFIG_SPLIT_SHRINKER_DEBUG
	{
		.procname	= "enable_split_shrinker",
		.data		= &g_split_shrinker_enable,
		.maxlen		= sizeof(g_split_shrinker_enable),
		.mode		= 0644,
		.proc_handler	= split_shrinker_sysctl_handler,
		.extra1		= &zero_int,
		.extra2		= &one_int,
	},
	{
		.procname	= "enable_register_skip_shrinker",
		.data		= &g_enable_register_skip_shrinker,
		.maxlen		= sizeof(g_enable_register_skip_shrinker),
		.mode		= 0644,
		.proc_handler	= register_skip_shrinker_sysctl_handler,
		.extra1		= &zero_int,
		.extra2		= &one_int,
	},
#endif
#endif
	{ }
};

static struct ctl_table sysctl_base_table[] = {
	{
		.procname	= "vm",
		.mode		= 0555,
		.child		= vm_table,
	},
	{ }
};

static int __init hw_mm_sysctl_init(void)
{
	register_sysctl_table(sysctl_base_table);
	return 0;
}
module_init(hw_mm_sysctl_init);
