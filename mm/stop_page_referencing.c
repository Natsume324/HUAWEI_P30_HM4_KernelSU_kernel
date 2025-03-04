 /*
  * Copyright (c) Huawei Technologies Co., Ltd. 2021-2022. All rights reserved.
  * Description: stop page_referenced() in advance to reduce vma walk.
  * Author: Gong Chen <gongchen4@huawei.com>
  * Create: 2021-12-09
  */
#include <linux/stop_page_referencing.h>

#include <linux/mm_inline.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/version.h>
#include <linux/sysctl.h>

enum {
	REF_TWO_OR_MORE = 2,
	REF_TOTAL,
	REF_STOP,
	REF_TYPE_MAX,
};

static int g_enable_stop_page_referencing = 1;

#if !(defined(SYSCTL_ZERO) && defined(SYSCTL_ONE))
static int sysctl_vals[] = { 0, 1, INT_MAX };
#define SYSCTL_ZERO	((void *)&sysctl_vals[0])
#define SYSCTL_ONE	((void *)&sysctl_vals[1])
#define SYSCTL_INT_MAX	((void *)&sysctl_vals[2])
#endif

static struct ctl_table stop_page_ref_table[] = {
	{
		.procname	= "enable_stop_page_referencing",
		.data		= &g_enable_stop_page_referencing,
		.maxlen		= sizeof(g_enable_stop_page_referencing),
		.mode		= 0660,
		.proc_handler	= proc_dointvec_minmax,
		.extra1		= SYSCTL_ZERO,
		.extra2		= SYSCTL_ONE,
	},
	{ }
};

static struct ctl_table vm_table[] = {
	{
		.procname	= "vm",
		.mode		= 0555,
		.child		= stop_page_ref_table,
	},
	{ }
};

#ifdef CONFIG_STOP_PAGE_REF_DEBUG
#define LIST_NUM  2
static atomic64_t page_ref_nums[LIST_NUM][REF_TYPE_MAX];

void count_ref_pages(int ref, enum page_ref_type type)
{
	if (unlikely(type > INACTIVE_REF))
		return;

	if (ref == 0 || ref == 1) {
		atomic64_inc(&page_ref_nums[type][ref]);
	} else {
		atomic64_inc(&page_ref_nums[type][REF_TWO_OR_MORE]);
		atomic64_add(ref, &page_ref_nums[type][REF_TOTAL]);
	}
}

static void inc_ref_stop_page(enum page_ref_type type)
{
	atomic64_inc(&page_ref_nums[type][REF_STOP]);
}
#endif

int get_page_ref_type(struct page *page, int ref_type)
{
	if (g_enable_stop_page_referencing == 0)
		return INVALID_REF;

	if (PageReferencing(page)) {
		ClearPageReferencing(page);
		return INVALID_REF;
	} else if (ref_type == ACTIVE_REF) {
		return page_is_file_cache(page) ? ref_type : INVALID_REF;
	}

	return ref_type;
}

static bool check_active_ref(struct page *page, unsigned long vm_flags,
	int referenced)
{
	if (!referenced)
		return false;

	if (vm_flags & VM_EXEC) {
		SetPageReferencing(page);
#ifdef CONFIG_STOP_PAGE_REF_DEBUG
		inc_ref_stop_page(ACTIVE_REF);
#endif
		return true;
	}
	return false;
}

static bool check_inactive_ref(struct page *page, unsigned long vm_flags,
	int referenced)
{
	if (referenced) {
#if (LINUX_VERSION_CODE < KERNEL_VERSION(5, 10, 0))
		if (PageSwapBacked(page))
			goto check_ok;
#endif
		if (PageReferenced(page) || referenced > 1)
			goto check_ok;

#if (LINUX_VERSION_CODE < KERNEL_VERSION(5, 10, 0))
		if (vm_flags & VM_EXEC)
			goto check_ok;
#else
		if (vm_flags & VM_EXEC && !PageSwapBacked(page))
			goto check_ok;
#endif
	}

	return false;

check_ok:
#ifdef CONFIG_STOP_PAGE_REF_DEBUG
	inc_ref_stop_page(INACTIVE_REF);
#endif
	SetPageReferencing(page);
	return true;
}

bool should_stop_page_ref(struct page *page, int referenced,
	int ref_type, unsigned long vm_flags)
{
	if (ref_type == ACTIVE_REF)
		return check_active_ref(page, vm_flags, referenced);

	if (ref_type == INACTIVE_REF)
		return check_inactive_ref(page, vm_flags, referenced);

	return false;
}

#ifdef CONFIG_STOP_PAGE_REF_DEBUG
static int page_ref_info_proc_show(struct seq_file *m, void *v)
{
	int i, j;
	const char *type[LIST_NUM] = {
		"active  ",
		"inactve ",
	};
	const char *info[REF_TYPE_MAX] = {
		"ref 0     :",
		"ref 1     :",
		"ref 2+    :",
		"ref total :",
		"ref stop  :",
	};

	for (j = 0; j < LIST_NUM; j++) {
		for (i = 0; i < REF_TYPE_MAX; i++)
			seq_printf(m, "%s %s %llu\n", type[j], info[i],
				atomic64_read(&page_ref_nums[j][i]));
	}

	return 0;
}

static int page_ref_info_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, page_ref_info_proc_show, NULL);
}

static const struct file_operations page_ref_info_fops = {
	.open		= page_ref_info_proc_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};
#endif

static int __init page_ref_info_init(void)
{
#ifdef CONFIG_STOP_PAGE_REF_DEBUG
	proc_create("page_ref_info", 0440, NULL, &page_ref_info_fops);
#endif
	register_sysctl_table(vm_table);
	return 0;
}
module_init(page_ref_info_init);
