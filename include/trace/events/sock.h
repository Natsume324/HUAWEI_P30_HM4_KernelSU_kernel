/* SPDX-License-Identifier: GPL-2.0 */
#undef TRACE_SYSTEM
#define TRACE_SYSTEM sock

#if !defined(_TRACE_SOCK_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_SOCK_H

#include <net/sock.h>
#include <linux/tracepoint.h>
#include <linux/tcp.h>

#define tcp_state_names			\
		EM(TCP_ESTABLISHED)		\
		EM(TCP_SYN_SENT)		\
		EM(TCP_SYN_RECV)		\
		EM(TCP_FIN_WAIT1)		\
		EM(TCP_FIN_WAIT2)		\
		EM(TCP_TIME_WAIT)		\
		EM(TCP_CLOSE)			\
		EM(TCP_CLOSE_WAIT)		\
		EM(TCP_LAST_ACK)		\
		EM(TCP_LISTEN)			\
		EM(TCP_CLOSING)			\
		EMe(TCP_NEW_SYN_RECV)

/* enums need to be exported to user space */
#undef EM
#undef EMe
#define EM(a)       TRACE_DEFINE_ENUM(a);
#define EMe(a)      TRACE_DEFINE_ENUM(a);

tcp_state_names

#undef EM
#undef EMe
#define EM(a)       { a, #a },
#define EMe(a)      { a, #a }

#define show_tcp_state_name(val)        \
	__print_symbolic(val, tcp_state_names)

TRACE_EVENT(sock_rcvqueue_full,

	TP_PROTO(struct sock *sk, struct sk_buff *skb),

	TP_ARGS(sk, skb),

	TP_STRUCT__entry(
		__field(int, rmem_alloc)
		__field(unsigned int, truesize)
		__field(int, sk_rcvbuf)
	),

	TP_fast_assign(
		__entry->rmem_alloc = atomic_read(&sk->sk_rmem_alloc);
		__entry->truesize   = skb->truesize;
		__entry->sk_rcvbuf  = sk->sk_rcvbuf;
	),

	TP_printk("rmem_alloc=%d truesize=%u sk_rcvbuf=%d",
		__entry->rmem_alloc, __entry->truesize, __entry->sk_rcvbuf)
);

TRACE_EVENT(sock_exceed_buf_limit,

	TP_PROTO(struct sock *sk, struct proto *prot, long allocated),

	TP_ARGS(sk, prot, allocated),

	TP_STRUCT__entry(
		__array(char, name, 32)
		__field(long *, sysctl_mem)
		__field(long, allocated)
		__field(int, sysctl_rmem)
		__field(int, rmem_alloc)
	),

	TP_fast_assign(
		strncpy(__entry->name, prot->name, 32);
		__entry->sysctl_mem = prot->sysctl_mem;
		__entry->allocated = allocated;
		__entry->sysctl_rmem = prot->sysctl_rmem[0];
		__entry->rmem_alloc = atomic_read(&sk->sk_rmem_alloc);
	),

	TP_printk("proto:%s sysctl_mem=%ld,%ld,%ld allocated=%ld "
		"sysctl_rmem=%d rmem_alloc=%d",
		__entry->name,
		__entry->sysctl_mem[0],
		__entry->sysctl_mem[1],
		__entry->sysctl_mem[2],
		__entry->allocated,
		__entry->sysctl_rmem,
		__entry->rmem_alloc)
);

#endif /* _TRACE_SOCK_H */

/* This part must be outside protection */
#include <trace/define_trace.h>
