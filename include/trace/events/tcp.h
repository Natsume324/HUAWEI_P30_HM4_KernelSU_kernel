/* SPDX-License-Identifier: GPL-2.0 */
#undef TRACE_SYSTEM
#define TRACE_SYSTEM tcp

#if !defined(_TRACE_TCP_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_TCP_H

#include <linux/ipv6.h>
#include <linux/tcp.h>
#include <linux/tracepoint.h>
#include <net/ipv6.h>
#include <net/tcp.h>

#ifdef CONFIG_TCP_TRACE
#define TP_STORE_V4MAPPED(__entry, saddr, daddr)		\
	do {							\
		struct in6_addr *pin6;				\
								\
		pin6 = (struct in6_addr *)__entry->saddr_v6;	\
		ipv6_addr_set_v4mapped(saddr, pin6);		\
		pin6 = (struct in6_addr *)__entry->daddr_v6;	\
		ipv6_addr_set_v4mapped(daddr, pin6);		\
	} while (0)

#if IS_ENABLED(CONFIG_IPV6)
#define TP_STORE_ADDRS(__entry, saddr, daddr, saddr6, daddr6)		\
	do {								\
		if (sk->sk_family == AF_INET6) {			\
			struct in6_addr *pin6;				\
									\
			pin6 = (struct in6_addr *)__entry->saddr_v6;	\
			*pin6 = saddr6;					\
			pin6 = (struct in6_addr *)__entry->daddr_v6;	\
			*pin6 = daddr6;					\
		} else {						\
			TP_STORE_V4MAPPED(__entry, saddr, daddr);	\
		}							\
	} while (0)
#else
#define TP_STORE_ADDRS(__entry, saddr, daddr, saddr6, daddr6)	\
	TP_STORE_V4MAPPED(__entry, saddr, daddr)
#endif

/*
 * tcp event with arguments sk and skb
 *
 * Note: this class requires a valid sk pointer; while skb pointer could
 *       be NULL.
 */
DECLARE_EVENT_CLASS(tcp_event_sk_skb,

	TP_PROTO(const struct sock *sk, const struct sk_buff *skb),

	TP_ARGS(sk, skb),

	TP_STRUCT__entry(
		__field(int, state)
		__field(int, owner_pid)
		__field(__u16, sport)
		__field(__u16, dport)
		__array(__u8, saddr, 4)
		__array(__u8, daddr, 4)
		__array(__u8, saddr_v6, 16)
		__array(__u8, daddr_v6, 16)
		__field(__u32, seq)
		__field(__u32, end)
	),

	TP_fast_assign(
		struct inet_sock *inet = inet_sk(sk);
		__be32 *p32;

		__entry->state = sk->sk_state;
#ifdef CONFIG_HUAWEI_KSTATE
		__entry->owner_pid = sk->sk_pid;
#endif

		__entry->sport = ntohs(inet->inet_sport);
		__entry->dport = ntohs(inet->inet_dport);

		p32 = (__be32 *) __entry->saddr;
		*p32 = inet->inet_saddr;

		p32 = (__be32 *) __entry->daddr;
		*p32 =  inet->inet_daddr;

		TP_STORE_ADDRS(__entry, inet->inet_saddr, inet->inet_daddr,
			      sk->sk_v6_rcv_saddr, sk->sk_v6_daddr);

		if (skb) {
			__entry->seq = TCP_SKB_CB(skb)->seq;
			__entry->end = TCP_SKB_CB(skb)->end_seq;
		}
	),

	TP_printk("pid=%d seq=%u end=%u sport=%hu dport=%hu saddr=%pI4 daddr=%pI4 saddrv6=%pI6c daddrv6=%pI6c state=%s",
		  __entry->owner_pid, __entry->seq, __entry->end,
		  __entry->sport, __entry->dport, __entry->saddr, __entry->daddr,
		  __entry->saddr_v6, __entry->daddr_v6,
		  show_tcp_state_name(__entry->state))
);

DEFINE_EVENT(tcp_event_sk_skb, tcp_retransmit_skb,

	TP_PROTO(const struct sock *sk, const struct sk_buff *skb),

	TP_ARGS(sk, skb)
);

/*
 * skb of trace_tcp_send_reset is the skb that caused RST. In case of
 * active reset, skb should be NULL
 */
DEFINE_EVENT(tcp_event_sk_skb, tcp_send_reset,

	TP_PROTO(const struct sock *sk, const struct sk_buff *skb),

	TP_ARGS(sk, skb)
);

/*
 * tcp event with arguments sk
 *
 * Note: this class requires a valid sk pointer.
 */
DECLARE_EVENT_CLASS(tcp_event_sk,

	TP_PROTO(struct sock *sk),

	TP_ARGS(sk),

	TP_STRUCT__entry(
		__field(int, owner_pid)
		__field(__u16, sport)
		__field(__u16, dport)
		__array(__u8, saddr, 4)
		__array(__u8, daddr, 4)
		__array(__u8, saddr_v6, 16)
		__array(__u8, daddr_v6, 16)
	),

	TP_fast_assign(
		struct inet_sock *inet = inet_sk(sk);
		__be32 *p32;

#ifdef CONFIG_HUAWEI_KSTATE
		__entry->owner_pid = sk->sk_pid;
#endif

		__entry->sport = ntohs(inet->inet_sport);
		__entry->dport = ntohs(inet->inet_dport);

		p32 = (__be32 *) __entry->saddr;
		*p32 = inet->inet_saddr;

		p32 = (__be32 *) __entry->daddr;
		*p32 =  inet->inet_daddr;

		TP_STORE_ADDRS(__entry, inet->inet_saddr, inet->inet_daddr,
			       sk->sk_v6_rcv_saddr, sk->sk_v6_daddr);
	),

	TP_printk("pid=%d sport=%hu dport=%hu saddr=%pI4 daddr=%pI4 saddrv6=%pI6c daddrv6=%pI6c",
		  __entry->owner_pid, __entry->sport, __entry->dport,
		  __entry->saddr, __entry->daddr,
		  __entry->saddr_v6, __entry->daddr_v6)
);

DEFINE_EVENT(tcp_event_sk, tcp_receive_reset,

	TP_PROTO(struct sock *sk),

	TP_ARGS(sk)
);

TRACE_EVENT(tcp_retransmit_synack,

	TP_PROTO(const struct sock *sk, const struct request_sock *req),

	TP_ARGS(sk, req),

	TP_STRUCT__entry(
		__field(const void *, req)
		__field(int, owner_pid)
		__field(__u16, sport)
		__field(__u16, dport)
		__array(__u8, saddr, 4)
		__array(__u8, daddr, 4)
		__array(__u8, saddr_v6, 16)
		__array(__u8, daddr_v6, 16)
	),

	TP_fast_assign(
		struct inet_request_sock *ireq = inet_rsk(req);
		__be32 *p32;

		__entry->req = req;
#ifdef CONFIG_HUAWEI_KSTATE
		__entry->owner_pid = sk->sk_pid;
#endif

		__entry->sport = ireq->ir_num;
		__entry->dport = ntohs(ireq->ir_rmt_port);

		p32 = (__be32 *) __entry->saddr;
		*p32 = ireq->ir_loc_addr;

		p32 = (__be32 *) __entry->daddr;
		*p32 = ireq->ir_rmt_addr;

		TP_STORE_ADDRS(__entry, ireq->ir_loc_addr, ireq->ir_rmt_addr,
			      ireq->ir_v6_loc_addr, ireq->ir_v6_rmt_addr);
	),

	TP_printk("pid=%d sport=%hu dport=%hu saddr=%pI4 daddr=%pI4 saddrv6=%pI6c daddrv6=%pI6c",
		  __entry->owner_pid, __entry->sport, __entry->dport,
		  __entry->saddr, __entry->daddr,
		  __entry->saddr_v6, __entry->daddr_v6)
);

TRACE_EVENT(tcp_rcv_retransmit,

	TP_PROTO(const struct sock *sk, const struct sk_buff *skb),

	TP_ARGS(sk, skb),

	TP_STRUCT__entry(
		__field(int, state)
		__field(int, owner_pid)
		__field(__u16, sport)
		__field(__u16, dport)
		__array(__u8, saddr, 4)
		__array(__u8, daddr, 4)
		__array(__u8, saddr_v6, 16)
		__array(__u8, daddr_v6, 16)
		__field(__u32, seq)
		__field(__u32, end)
	),

	TP_fast_assign(
		struct inet_sock *inet = inet_sk(sk);
		__be32 *p32;

		__entry->state = sk->sk_state;
#ifdef CONFIG_HUAWEI_KSTATE
		__entry->owner_pid = sk->sk_pid;
#endif
		__entry->state = sk->sk_state;

		__entry->sport = ntohs(inet->inet_sport);
		__entry->dport = ntohs(inet->inet_dport);

		p32 = (__be32 *) __entry->saddr;
		*p32 = inet->inet_saddr;

		p32 = (__be32 *) __entry->daddr;
		*p32 =  inet->inet_daddr;

		TP_STORE_ADDRS(__entry, inet->inet_saddr, inet->inet_daddr,
			      sk->sk_v6_rcv_saddr, sk->sk_v6_daddr);

		__entry->seq = TCP_SKB_CB(skb)->seq;
		__entry->end = TCP_SKB_CB(skb)->end_seq;
	),

	TP_printk("pid=%d seq=%u end=%u sport=%hu dport=%hu saddr=%pI4 daddr=%pI4 saddrv6=%pI6c daddrv6=%pI6c state=%s",
		  __entry->owner_pid, __entry->seq, __entry->end,
          __entry->sport, __entry->dport, __entry->saddr, __entry->daddr,
		  __entry->saddr_v6, __entry->daddr_v6,
		  show_tcp_state_name(__entry->state))
);
#else
#define trace_tcp_retransmit_skb(sk, skb)
#define trace_tcp_send_reset(sk, skb)
#define trace_tcp_receive_reset(sk)
#define trace_tcp_retransmit_synack(sk, req)
#define trace_tcp_rcv_retransmit(sk, skb)
#endif /* CONFIG_TCP_TRACE */

#endif /* _TRACE_TCP_H */

/* This part must be outside protection */
#include <trace/define_trace.h>
