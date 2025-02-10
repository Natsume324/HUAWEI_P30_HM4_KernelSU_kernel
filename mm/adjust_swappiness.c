// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023. All rights reserved.
 * Description: adjust kswapd swappiness when it running with high load.
 * Author: Mi Sasa <misasa@huawei.com>
 * Create: 2023-11-07
 */

#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/sched.h>
#include <linux/swap.h>
#include <linux/sysctl.h>
#include <trace/events/vmscan.h>

#include "internal.h"

static int zero_int = 0;
static int one_hundred = 100;
static int two_hundred = 200;

enum track_policy {
	TRACK_BIG_CPU = 1,
	TRACK_FAST_CPU,
	TRACK_ALL_CPU,
};

/* Policy for kswapd high load swappiness adjustment, 0:close, 1:BIG_CPU, 2:FAST_CPU, 3:ALL_CPU. */
static int kswapd_track_load_policy = 0;

/* Swappiness ratio for kswapd when it running with high load, expect 90. */
static int kswapd_high_load_swappiness_ratio = 90;

/* Threshold for determining whether kswapd is high load. The capacity of a big core is 1024. */
static int kswapd_high_load_threshold_value = 800;

#ifdef CONFIG_KSWAPD_DEBUG
static atomic64_t kswapd_called_cnt = ATOMIC64_INIT(0);
static atomic64_t kswapd_in_dr_cnt = ATOMIC64_INIT(0);
static atomic64_t kswapd_high_load_cnt = ATOMIC64_INIT(0);
#endif

#ifdef CONFIG_KSWAPD_DEBUG
static void inc_kswapd_called_cnt(void)
{
	atomic64_inc(&kswapd_called_cnt);
}

static void inc_kswapd_in_dr_cnt(void)
{
	atomic64_inc(&kswapd_in_dr_cnt);
}

static void inc_kswapd_high_load_cnt(void)
{
	atomic64_inc(&kswapd_high_load_cnt);
}

static int kswapd_high_load_debug_show(struct seq_file *m, void *v)
{
	seq_printf(m, "kswapd_called_cnt: %lld\n",
			atomic64_read(&kswapd_called_cnt));
	seq_printf(m, "kswapd_in_dr_cnt: %lld\n",
			atomic64_read(&kswapd_in_dr_cnt));
	seq_printf(m, "kswapd_high_load_cnt: %lld\n",
			atomic64_read(&kswapd_high_load_cnt));
	return 0;
}
#else
static inline void inc_kswapd_called_cnt(void) {}
static inline void inc_kswapd_in_dr_cnt(void) {}
static inline void inc_kswapd_high_load_cnt(void) {}
#endif

static struct ctl_table vm_table[] = {
	{
		.procname	= "kswapd_track_load_policy",
		.data		= &kswapd_track_load_policy,
		.maxlen		= sizeof(kswapd_track_load_policy),
		.mode		= 0644,
		.proc_handler	= proc_dointvec_minmax,
		.extra1		= &zero_int,
	},
	{
		.procname	= "kswapd_high_load_swappiness_ratio",
		.data		= &kswapd_high_load_swappiness_ratio,
		.maxlen		= sizeof(kswapd_high_load_swappiness_ratio),
		.mode		= 0644,
		.proc_handler	= proc_dointvec_minmax,
		.extra1		= &zero_int,
		.extra2		= &one_hundred,
	},
	{
		.procname	= "kswapd_high_load_threshold_value",
		.data		= &kswapd_high_load_threshold_value,
		.maxlen		= sizeof(kswapd_high_load_threshold_value),
		.mode		= 0644,
		.proc_handler	= proc_dointvec_minmax,
		.extra1		= &zero_int,
	},
	{ },
};

static struct ctl_table sys_table[] = {
	{
		.procname	= "vm",
		.mode		= 0555,
		.child		= vm_table,
	},
	{ },
};

#ifdef CONFIG_KSWAPD_DEBUG
static int kswapd_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, kswapd_high_load_debug_show, NULL);
}

static const struct file_operations kswapd_proc_fops = {
	.open		= kswapd_proc_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};
#endif

static int __init kswapd_sysctl_init(void)
{
	register_sysctl_table(sys_table);

#ifdef CONFIG_KSWAPD_DEBUG
	proc_create("kswapd_high_load_debug", 0, NULL, &kswapd_proc_fops);
#endif

	return 0;
}

module_init(kswapd_sysctl_init);

static bool is_kswapd_high_load(void)
{
	bool fit_cpu = false;
	int policy;
	unsigned int cpu_id;
	unsigned long current_task_util;

	cpu_id = smp_processor_id();
	switch (kswapd_track_load_policy) {
	case TRACK_BIG_CPU:
		fit_cpu = is_max_cap_orig_cpu(cpu_id);
		break;
	case TRACK_FAST_CPU:
		fit_cpu = (test_fast_cpu(cpu_id) == 1);
		break;
	case TRACK_ALL_CPU:
		fit_cpu = true;
		break;
	default:
		return false;
	}

	current_task_util = task_util(current);
	trace_mm_kswapd_current_task_util(current_task_util, fit_cpu, cpu_id);
	return fit_cpu && current_task_util > kswapd_high_load_threshold_value;
}

int adjust_swappiness(int swappiness)
{
	if (!current_is_kswapd())
#ifdef CONFIG_DIRECT_SWAPPINESS
		return direct_vm_swappiness;
#else
		return swappiness;
#endif

	inc_kswapd_called_cnt();

#ifdef CONFIG_OPTIMIZE_MM_AQ
	if (is_in_direct_reclaim()) {
		inc_kswapd_in_dr_cnt();
		return 0;
	}
#endif

	if (is_kswapd_high_load()) {
		inc_kswapd_high_load_cnt();
		return (swappiness * kswapd_high_load_swappiness_ratio) / 100;
	}

	return swappiness;
}