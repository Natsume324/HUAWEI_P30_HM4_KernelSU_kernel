/* SPDX-License-Identifier: GPL-2.0 */
#undef TRACE_SYSTEM
#define TRACE_SYSTEM vmscan

#if !defined(_TRACE_VMSCAN_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_VMSCAN_H

#include <linux/types.h>
#include <linux/tracepoint.h>
#include <linux/mm.h>
#include <linux/memcontrol.h>
#include <trace/events/mmflags.h>

#define RECLAIM_WB_ANON		0x0001u
#define RECLAIM_WB_FILE		0x0002u
#define RECLAIM_WB_MIXED	0x0010u
#define RECLAIM_WB_SYNC		0x0004u /* Unused, all reclaim async */
#define RECLAIM_WB_ASYNC	0x0008u
#define RECLAIM_WB_LRU		(RECLAIM_WB_ANON|RECLAIM_WB_FILE)

#define show_reclaim_flags(flags)				\
	(flags) ? __print_flags(flags, "|",			\
		{RECLAIM_WB_ANON,	"RECLAIM_WB_ANON"},	\
		{RECLAIM_WB_FILE,	"RECLAIM_WB_FILE"},	\
		{RECLAIM_WB_MIXED,	"RECLAIM_WB_MIXED"},	\
		{RECLAIM_WB_SYNC,	"RECLAIM_WB_SYNC"},	\
		{RECLAIM_WB_ASYNC,	"RECLAIM_WB_ASYNC"}	\
		) : "RECLAIM_WB_NONE"

#define trace_reclaim_flags(page) ( \
	(page_is_file_cache(page) ? RECLAIM_WB_FILE : RECLAIM_WB_ANON) | \
	(RECLAIM_WB_ASYNC) \
	)

#define trace_shrink_flags(file) \
	( \
		(file ? RECLAIM_WB_FILE : RECLAIM_WB_ANON) | \
		(RECLAIM_WB_ASYNC) \
	)

TRACE_EVENT(mm_vmscan_kswapd_sleep,

	TP_PROTO(int nid),

	TP_ARGS(nid),

	TP_STRUCT__entry(
		__field(	int,	nid	)
	),

	TP_fast_assign(
		__entry->nid	= nid;
	),

	TP_printk("nid=%d", __entry->nid)
);

TRACE_EVENT(mm_vmscan_kswapd_wake,

	TP_PROTO(int nid, int zid, int order),

	TP_ARGS(nid, zid, order),

	TP_STRUCT__entry(
		__field(	int,	nid	)
		__field(	int,	zid	)
		__field(	int,	order	)
	),

	TP_fast_assign(
		__entry->nid	= nid;
		__entry->zid    = zid;
		__entry->order	= order;
	),

	TP_printk("nid=%d zid=%d order=%d", __entry->nid, __entry->zid, __entry->order)
);

TRACE_EVENT(mm_vmscan_wakeup_kswapd,

	TP_PROTO(int nid, int zid, int order),

	TP_ARGS(nid, zid, order),

	TP_STRUCT__entry(
		__field(	int,		nid	)
		__field(	int,		zid	)
		__field(	int,		order	)
	),

	TP_fast_assign(
		__entry->nid		= nid;
		__entry->zid		= zid;
		__entry->order		= order;
	),

	TP_printk("nid=%d zid=%d order=%d",
		__entry->nid,
		__entry->zid,
		__entry->order)
);

#ifdef CONFIG_HYPERHOLD
TRACE_EVENT(mm_zswapd_wake,

	TP_PROTO(int nid),

	TP_ARGS(nid),

	TP_STRUCT__entry(
		__field(	int,	nid	)
	),

	TP_fast_assign(
		__entry->nid = nid;
	),

	TP_printk("nid=%d", __entry->nid)
);

TRACE_EVENT(mm_zswapd_sleep,

	TP_PROTO(int nid),

	TP_ARGS(nid),

	TP_STRUCT__entry(
		__field(	int,	nid	)
	),

	TP_fast_assign(
		__entry->nid = nid;
	),

	TP_printk("nid=%d", __entry->nid)
);
#endif

DECLARE_EVENT_CLASS(mm_vmscan_direct_reclaim_begin_template,

	TP_PROTO(int order, int may_writepage, gfp_t gfp_flags, int classzone_idx),

	TP_ARGS(order, may_writepage, gfp_flags, classzone_idx),

	TP_STRUCT__entry(
		__field(	int,	order		)
		__field(	int,	may_writepage	)
		__field(	gfp_t,	gfp_flags	)
		__field(	int,	classzone_idx	)
	),

	TP_fast_assign(
		__entry->order		= order;
		__entry->may_writepage	= may_writepage;
		__entry->gfp_flags	= gfp_flags;
		__entry->classzone_idx	= classzone_idx;
	),

	TP_printk("order=%d may_writepage=%d gfp_flags=%s classzone_idx=%d",
		__entry->order,
		__entry->may_writepage,
		show_gfp_flags(__entry->gfp_flags),
		__entry->classzone_idx)
);

DEFINE_EVENT(mm_vmscan_direct_reclaim_begin_template, mm_vmscan_direct_reclaim_begin,

	TP_PROTO(int order, int may_writepage, gfp_t gfp_flags, int classzone_idx),

	TP_ARGS(order, may_writepage, gfp_flags, classzone_idx)
);

DEFINE_EVENT(mm_vmscan_direct_reclaim_begin_template, mm_vmscan_memcg_reclaim_begin,

	TP_PROTO(int order, int may_writepage, gfp_t gfp_flags, int classzone_idx),

	TP_ARGS(order, may_writepage, gfp_flags, classzone_idx)
);

DEFINE_EVENT(mm_vmscan_direct_reclaim_begin_template, mm_vmscan_memcg_softlimit_reclaim_begin,

	TP_PROTO(int order, int may_writepage, gfp_t gfp_flags, int classzone_idx),

	TP_ARGS(order, may_writepage, gfp_flags, classzone_idx)
);

DECLARE_EVENT_CLASS(mm_vmscan_direct_reclaim_end_template,

	TP_PROTO(unsigned long nr_reclaimed),

	TP_ARGS(nr_reclaimed),

	TP_STRUCT__entry(
		__field(	unsigned long,	nr_reclaimed	)
	),

	TP_fast_assign(
		__entry->nr_reclaimed	= nr_reclaimed;
	),

	TP_printk("nr_reclaimed=%lu", __entry->nr_reclaimed)
);

DEFINE_EVENT(mm_vmscan_direct_reclaim_end_template, mm_vmscan_direct_reclaim_end,

	TP_PROTO(unsigned long nr_reclaimed),

	TP_ARGS(nr_reclaimed)
);

DEFINE_EVENT(mm_vmscan_direct_reclaim_end_template, mm_vmscan_memcg_reclaim_end,

	TP_PROTO(unsigned long nr_reclaimed),

	TP_ARGS(nr_reclaimed)
);

DEFINE_EVENT(mm_vmscan_direct_reclaim_end_template, mm_vmscan_memcg_softlimit_reclaim_end,

	TP_PROTO(unsigned long nr_reclaimed),

	TP_ARGS(nr_reclaimed)
);

TRACE_EVENT(mm_shrink_slab_start,
#ifndef CONFIG_SPLIT_SHRINKER
	TP_PROTO(struct shrinker *shr, struct shrink_control *sc,
		long nr_objects_to_shrink, unsigned long pgs_scanned,
		unsigned long lru_pgs, unsigned long cache_items,
		unsigned long long delta, unsigned long total_scan),

	TP_ARGS(shr, sc, nr_objects_to_shrink, pgs_scanned, lru_pgs,
		cache_items, delta, total_scan),
#else
	TP_PROTO(struct shrinker *shr, struct shrink_control *sc,
		long nr_objects_to_shrink, unsigned long cache_items,
		unsigned long long delta, unsigned long total_scan,
		int priority),

	TP_ARGS(shr, sc, nr_objects_to_shrink, cache_items, delta, total_scan,
		priority),
#endif

	TP_STRUCT__entry(
		__field(struct shrinker *, shr)
		__field(void *, shrink)
		__field(int, nid)
		__field(long, nr_objects_to_shrink)
		__field(gfp_t, gfp_flags)
#ifndef CONFIG_SPLIT_SHRINKER
		__field(unsigned long, pgs_scanned)
		__field(unsigned long, lru_pgs)
#endif
		__field(unsigned long, cache_items)
		__field(unsigned long long, delta)
		__field(unsigned long, total_scan)
#ifdef CONFIG_SPLIT_SHRINKER
		__field(int, priority)
#endif
	),

	TP_fast_assign(
		__entry->shr = shr;
		__entry->shrink = shr->scan_objects;
		__entry->nid = sc->nid;
		__entry->nr_objects_to_shrink = nr_objects_to_shrink;
		__entry->gfp_flags = sc->gfp_mask;
#ifndef CONFIG_SPLIT_SHRINKER
		__entry->pgs_scanned = pgs_scanned;
		__entry->lru_pgs = lru_pgs;
#endif
		__entry->cache_items = cache_items;
		__entry->delta = delta;
		__entry->total_scan = total_scan;
#ifdef CONFIG_SPLIT_SHRINKER
		__entry->priority = priority;
#endif
	),

#ifndef CONFIG_SPLIT_SHRINKER
	TP_printk("%pF %p: nid: %d objects to shrink %ld gfp_flags %s pgs_scanned %ld lru_pgs %ld cache items %ld delta %lld total_scan %ld",
#else
	TP_printk("%pF %p: nid: %d objects to shrink %ld gfp_flags %s cache items %ld delta %lld total_scan %ld priority %d",
#endif
		__entry->shrink,
		__entry->shr,
		__entry->nid,
		__entry->nr_objects_to_shrink,
		show_gfp_flags(__entry->gfp_flags),
#ifndef CONFIG_SPLIT_SHRINKER
		__entry->pgs_scanned,
		__entry->lru_pgs,
#endif
		__entry->cache_items,
		__entry->delta,
#ifndef CONFIG_SPLIT_SHRINKER
		__entry->total_scan)
#else
		__entry->total_scan,
		__entry->priority)
#endif
);

TRACE_EVENT(mm_shrink_slab_end,
	TP_PROTO(struct shrinker *shr, int nid, int shrinker_retval,
		long unused_scan_cnt, long new_scan_cnt, long total_scan),

	TP_ARGS(shr, nid, shrinker_retval, unused_scan_cnt, new_scan_cnt,
		total_scan),

	TP_STRUCT__entry(
		__field(struct shrinker *, shr)
		__field(int, nid)
		__field(void *, shrink)
		__field(long, unused_scan)
		__field(long, new_scan)
		__field(int, retval)
		__field(long, total_scan)
	),

	TP_fast_assign(
		__entry->shr = shr;
		__entry->nid = nid;
		__entry->shrink = shr->scan_objects;
		__entry->unused_scan = unused_scan_cnt;
		__entry->new_scan = new_scan_cnt;
		__entry->retval = shrinker_retval;
		__entry->total_scan = total_scan;
	),

	TP_printk("%pF %p: nid: %d unused scan count %ld new scan count %ld total_scan %ld last shrinker return val %d",
		__entry->shrink,
		__entry->shr,
		__entry->nid,
		__entry->unused_scan,
		__entry->new_scan,
		__entry->total_scan,
		__entry->retval)
);

TRACE_EVENT(mm_vmscan_lru_isolate,
	TP_PROTO(int classzone_idx,
		int order,
		unsigned long nr_requested,
		unsigned long nr_scanned,
		unsigned long nr_skipped,
		unsigned long nr_taken,
		isolate_mode_t isolate_mode,
		int lru),

	TP_ARGS(classzone_idx, order, nr_requested, nr_scanned, nr_skipped, nr_taken, isolate_mode, lru),

	TP_STRUCT__entry(
		__field(int, classzone_idx)
		__field(int, order)
		__field(unsigned long, nr_requested)
		__field(unsigned long, nr_scanned)
		__field(unsigned long, nr_skipped)
		__field(unsigned long, nr_taken)
		__field(isolate_mode_t, isolate_mode)
		__field(int, lru)
	),

	TP_fast_assign(
		__entry->classzone_idx = classzone_idx;
		__entry->order = order;
		__entry->nr_requested = nr_requested;
		__entry->nr_scanned = nr_scanned;
		__entry->nr_skipped = nr_skipped;
		__entry->nr_taken = nr_taken;
		__entry->isolate_mode = isolate_mode;
		__entry->lru = lru;
	),

	TP_printk("isolate_mode=%d classzone=%d order=%d nr_requested=%lu nr_scanned=%lu nr_skipped=%lu nr_taken=%lu lru=%s",
		__entry->isolate_mode,
		__entry->classzone_idx,
		__entry->order,
		__entry->nr_requested,
		__entry->nr_scanned,
		__entry->nr_skipped,
		__entry->nr_taken,
		__print_symbolic(__entry->lru, LRU_NAMES))
);

TRACE_EVENT(mm_vmscan_writepage,

	TP_PROTO(struct page *page),

	TP_ARGS(page),

	TP_STRUCT__entry(
		__field(unsigned long, pfn)
		__field(int, reclaim_flags)
	),

	TP_fast_assign(
		__entry->pfn = page_to_pfn(page);
		__entry->reclaim_flags = trace_reclaim_flags(page);
	),

	TP_printk("page=%p pfn=%lu flags=%s",
		pfn_to_page(__entry->pfn),
		__entry->pfn,
		show_reclaim_flags(__entry->reclaim_flags))
);

TRACE_EVENT(mm_vmscan_lru_shrink_inactive,

	TP_PROTO(int nid,
		unsigned long nr_scanned, unsigned long nr_reclaimed,
		unsigned long nr_dirty, unsigned long nr_writeback,
		unsigned long nr_congested, unsigned long nr_immediate,
		unsigned long nr_activate, unsigned long nr_ref_keep,
		unsigned long nr_unmap_fail,
		int priority, int file),

	TP_ARGS(nid, nr_scanned, nr_reclaimed, nr_dirty, nr_writeback,
		nr_congested, nr_immediate, nr_activate, nr_ref_keep,
		nr_unmap_fail, priority, file),

	TP_STRUCT__entry(
		__field(int, nid)
		__field(unsigned long, nr_scanned)
		__field(unsigned long, nr_reclaimed)
		__field(unsigned long, nr_dirty)
		__field(unsigned long, nr_writeback)
		__field(unsigned long, nr_congested)
		__field(unsigned long, nr_immediate)
		__field(unsigned long, nr_activate)
		__field(unsigned long, nr_ref_keep)
		__field(unsigned long, nr_unmap_fail)
		__field(int, priority)
		__field(int, reclaim_flags)
	),

	TP_fast_assign(
		__entry->nid = nid;
		__entry->nr_scanned = nr_scanned;
		__entry->nr_reclaimed = nr_reclaimed;
		__entry->nr_dirty = nr_dirty;
		__entry->nr_writeback = nr_writeback;
		__entry->nr_congested = nr_congested;
		__entry->nr_immediate = nr_immediate;
		__entry->nr_activate = nr_activate;
		__entry->nr_ref_keep = nr_ref_keep;
		__entry->nr_unmap_fail = nr_unmap_fail;
		__entry->priority = priority;
		__entry->reclaim_flags = trace_shrink_flags(file);
	),

	TP_printk("nid=%d nr_scanned=%ld nr_reclaimed=%ld nr_dirty=%ld nr_writeback=%ld nr_congested=%ld nr_immediate=%ld nr_activate=%ld nr_ref_keep=%ld nr_unmap_fail=%ld priority=%d flags=%s",
		__entry->nid,
		__entry->nr_scanned, __entry->nr_reclaimed,
		__entry->nr_dirty, __entry->nr_writeback,
		__entry->nr_congested, __entry->nr_immediate,
		__entry->nr_activate, __entry->nr_ref_keep,
		__entry->nr_unmap_fail, __entry->priority,
		show_reclaim_flags(__entry->reclaim_flags))
);

TRACE_EVENT(mm_vmscan_lru_zswapd_shrink_active,

	TP_PROTO(int nid, unsigned long nr_taken,
		unsigned long nr_deactivated,
		int priority),

	TP_ARGS(nid, nr_taken, nr_deactivated, priority),

	TP_STRUCT__entry(
		__field(int, nid)
		__field(unsigned long, nr_taken)
		__field(unsigned long, nr_deactivated)
		__field(int, priority)
	),

	TP_fast_assign(
		__entry->nid = nid;
		__entry->nr_taken = nr_taken;
		__entry->nr_deactivated = nr_deactivated;
		__entry->priority = priority;
	),

	TP_printk("nid=%d nr_taken=%ld nr_deactivated=%ld priority=%d",
		__entry->nid,
		__entry->nr_taken,
		__entry->nr_deactivated,
		__entry->priority)
);

TRACE_EVENT(mm_vmscan_lru_shrink_active,

	TP_PROTO(int nid, unsigned long nr_taken,
		unsigned long nr_active, unsigned long nr_deactivated,
		unsigned long nr_referenced, int priority, int file),

	TP_ARGS(nid, nr_taken, nr_active, nr_deactivated, nr_referenced, priority, file),

	TP_STRUCT__entry(
		__field(int, nid)
		__field(unsigned long, nr_taken)
		__field(unsigned long, nr_active)
		__field(unsigned long, nr_deactivated)
		__field(unsigned long, nr_referenced)
		__field(int, priority)
		__field(int, reclaim_flags)
	),

	TP_fast_assign(
		__entry->nid = nid;
		__entry->nr_taken = nr_taken;
		__entry->nr_active = nr_active;
		__entry->nr_deactivated = nr_deactivated;
		__entry->nr_referenced = nr_referenced;
		__entry->priority = priority;
		__entry->reclaim_flags = trace_shrink_flags(file);
	),

	TP_printk("nid=%d nr_taken=%ld nr_active=%ld nr_deactivated=%ld nr_referenced=%ld priority=%d flags=%s",
		__entry->nid,
		__entry->nr_taken,
		__entry->nr_active, __entry->nr_deactivated, __entry->nr_referenced,
		__entry->priority,
		show_reclaim_flags(__entry->reclaim_flags))
);

TRACE_EVENT(mm_vmscan_inactive_list_is_low,

	TP_PROTO(int nid, int reclaim_idx,
		unsigned long total_inactive, unsigned long inactive,
		unsigned long total_active, unsigned long active,
		unsigned long ratio, int file),

	TP_ARGS(nid, reclaim_idx, total_inactive, inactive, total_active, active, ratio, file),

	TP_STRUCT__entry(
		__field(int, nid)
		__field(int, reclaim_idx)
		__field(unsigned long, total_inactive)
		__field(unsigned long, inactive)
		__field(unsigned long, total_active)
		__field(unsigned long, active)
		__field(unsigned long, ratio)
		__field(int, reclaim_flags)
	),

	TP_fast_assign(
		__entry->nid = nid;
		__entry->reclaim_idx = reclaim_idx;
		__entry->total_inactive = total_inactive;
		__entry->inactive = inactive;
		__entry->total_active = total_active;
		__entry->active = active;
		__entry->ratio = ratio;
		__entry->reclaim_flags = trace_shrink_flags(file) & RECLAIM_WB_LRU;
	),

	TP_printk("nid=%d reclaim_idx=%d total_inactive=%ld inactive=%ld total_active=%ld active=%ld ratio=%ld flags=%s",
		__entry->nid,
		__entry->reclaim_idx,
		__entry->total_inactive, __entry->inactive,
		__entry->total_active, __entry->active,
		__entry->ratio,
		show_reclaim_flags(__entry->reclaim_flags))
);

#ifdef CONFIG_ADJUST_SWAPPINESS
TRACE_EVENT(mm_kswapd_current_task_util,
		TP_PROTO(unsigned long current_task_util, bool fit_cpu, unsigned int cpu_id),

		TP_ARGS(current_task_util, fit_cpu, cpu_id),

		TP_STRUCT__entry(
			__field(unsigned long, current_task_util)
			__field(bool, fit_cpu)
			__field(unsigned int, cpu_id)
		),

		TP_fast_assign(
			__entry->current_task_util = current_task_util;
			__entry->fit_cpu = fit_cpu;
			__entry->cpu_id = cpu_id;
		),

		TP_printk("kswapd_current_task_util=%lu, fit_cpu=%d, cpu_id=%u",
			__entry->current_task_util, __entry->fit_cpu, __entry->cpu_id)
);
#endif

#endif /* _TRACE_VMSCAN_H */

/* This part must be outside protection */
#include <trace/define_trace.h>
