/*
 * net/ndisc.h
 *
 * (C) Copyright 2013 Allied Telesis Labs NZ
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

/* IPv6 destination address of packet waiting for ND */
extern struct in6_addr net_nd_sol_packet_ip6;
/* MAC destination address of packet waiting for ND */
extern uchar *net_nd_packet_mac;
/* pointer to packet waiting to be transmitted after ND is resolved */
extern uchar *net_nd_tx_packet;
/* size of packet waiting to be transmitted */
extern int net_nd_tx_packet_size;
/* the timer for ND resolution */
extern ulong net_nd_timer_start;
/* the number of requests we have sent so far */
extern int net_nd_try;

void ndisc_init(void);
void ndisc_receive(struct ethernet_hdr *et, struct ip6_hdr *ip6, int len);
void ndisc_request(void);
int ndisc_timeout_check(void);
