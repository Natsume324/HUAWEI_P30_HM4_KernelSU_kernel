/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2022-2022. All rights reserved.
 * Description: Provide empty definition for deadline API when DEADLINE_DISABLE is open.
 * Author: Zhang Kuangqi <zhangkuangqi1@huawei.com>
 * Create: 2023-04-18
 */
#include <securec.h>
#include "sched.h"

struct dl_bandwidth def_dl_bandwidth;
const struct sched_class dl_sched_class;

void dl_change_utilization(struct task_struct *p, u64 new_bw)
{
	return;
}

void init_dl_bandwidth(struct dl_bandwidth *dl_b, u64 period, u64 runtime)
{
	return;
}

void init_dl_bw(struct dl_bw *dl_b)
{
	return;
}

void init_dl_rq(struct dl_rq *dl_rq)
{
	return;
}

void init_dl_task_timer(struct sched_dl_entity *dl_se)
{
	return;
}

void init_dl_inactive_task_timer(struct sched_dl_entity *dl_se)
{
	return;
}

#ifdef CONFIG_SMP
void init_sched_dl_class(void)
{
	return;
}

int dl_task_can_attach(struct task_struct *p, const struct cpumask *cs_cpus_allowed)
{
	return -1;
}

int dl_cpuset_cpumask_can_shrink(const struct cpumask *cur, const struct cpumask *trial)
{
	return 0;
}

bool dl_cpu_busy(unsigned int cpu)
{
	return false;
}
#endif /* CONFIG_SMP */

int sched_dl_global_validate(void)
{
	return 0;
}

void init_dl_rq_bw_ratio(struct dl_rq *dl_rq)
{
	return;
}

void sched_dl_do_global(void)
{
	return;
}

int sched_dl_overflow(struct task_struct *p, int policy, const struct sched_attr *attr)
{
	return 0;
}

void __setparam_dl(struct task_struct *p, const struct sched_attr *attr)
{
	return;
}

void __getparam_dl(struct task_struct *p, struct sched_attr *attr)
{
	return;
}

bool __checkparam_dl(const struct sched_attr *attr)
{
	return false;
}

void __dl_clear_params(struct task_struct *p)
{
	int ret;
	struct sched_dl_entity *dl_se = NULL;

	dl_se = &p->dl;
	ret = memset_s(dl_se, sizeof(struct sched_dl_entity), 0, sizeof(struct sched_dl_entity));
	if (ret != EOK)
		pr_err("%s: memset_s fail\n", __func__);
}

bool dl_param_changed(struct task_struct *p, const struct sched_attr *attr)
{
	return false;
}

#ifdef CONFIG_SCHED_DEBUG
void print_dl_stats(struct seq_file *m, int cpu)
{
	return;
}
#endif /* CONFIG_SCHED_DEBUG */
