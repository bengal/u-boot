/*
 * Simple IPv6 network layer implementation.
 *
 * Based and/or adapted from the IPv4 network layer in net.[hc]
 *
 * (C) Copyright 2013 Allied Telesis Labs NZ
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

/*
 * General Desription:
 *
 * The user interface supports commands for TFTP6.
 * Also, we support Neighbour discovery internally. Depending on available
 * data, these interact as follows:
 *
 * Neighbour Discovery:
 *
 *      Prerequisites:  - own ethernet address
 *                      - own IPv6 address
 *                      - TFTP server IPv6 address
 *      We want:        - TFTP server ethernet address
 *      Next step:      TFTP
 *
 * TFTP over IPv6:
 *
 *      Prerequisites:  - own ethernet address
 *                      - own IPv6 address
 *                      - TFTP server IPv6 address
 *                      - TFTP server ethernet address
 *                      - name of bootfile (if unknown, we use a default name
 *                        derived from our own IPv6 address)
 *      We want:        - load the boot file
 *      Next step:      none
 *
 */
#define DEBUG
#include <common.h>
#include <env.h>
#include <env_internal.h>
#include <malloc.h>
#include <net.h>
#include <net6.h>
#include "ndisc.h"

/* NULL IPv6 address */
struct in6_addr const net_null_addr_ip6 = ZERO_IPV6_ADDR;
/* Our gateway's IPv6 address */
struct in6_addr net_gateway6 = ZERO_IPV6_ADDR;
/* Our IPv6 addr (0 = unknown) */
struct in6_addr net_ip6 = ZERO_IPV6_ADDR;
/* Our link local IPv6 addr (0 = unknown) */
struct in6_addr net_link_local_ip6 = ZERO_IPV6_ADDR;
/* set server IPv6 addr (0 = unknown) */
struct in6_addr net_server_ip6 = ZERO_IPV6_ADDR;
/* The prefix length of our network */
u_int32_t net_prefix_length;

static int on_ip6addr(const char *name, const char *value, enum env_op op,
		      int flags)
{
	char *v, *s, *strcopy;
	int i;

	if (flags & H_PROGRAMMATIC)
		return 0;

	if (op == env_op_delete) {
		net_prefix_length = 0;
		net_copy_ip6(&net_ip6, &net_null_addr_ip6);
		return 0;
	}

	strcopy = strdup(value);
	if (strcopy == NULL)
		return -1;

	net_prefix_length = 128;
	i = 0;
	s = strcopy;
	while (s) {
		v = strsep(&s, "/");
		if (!v)
			break;

		switch (i++) {
		case 0:
			string_to_ip6(v, &net_ip6);
			break;
		case 1:
			net_prefix_length = simple_strtoul(v, NULL, 10);
			break;
		default:
			break;
		}
	}
	free(strcopy);

	return 0;
}
U_BOOT_ENV_CALLBACK(ip6addr, on_ip6addr);

static int on_gatewayip6(const char *name, const char *value, enum env_op op,
			 int flags)
{
	if (flags & H_PROGRAMMATIC)
		return 0;

	return string_to_ip6(value, &net_gateway6);
}
U_BOOT_ENV_CALLBACK(gatewayip6, on_gatewayip6);

static int on_serverip6(const char *name, const char *value, enum env_op op,
			int flags)
{
	if (flags & H_PROGRAMMATIC)
		return 0;

	return string_to_ip6(value, &net_server_ip6);
}
U_BOOT_ENV_CALLBACK(serverip6, on_serverip6);

int ip6_is_unspecified_addr(struct in6_addr *addr)
{
	return (addr->s6_addr32[0] | addr->s6_addr32[1] |
		addr->s6_addr32[2] | addr->s6_addr32[3]) == 0;
}

/**
 * We have 2 addresses that we should respond to. A link
 * local address and a global address. This returns true
 * if the specified address matches either of these.
 */
int ip6_is_our_addr(struct in6_addr *addr)
{
	return memcmp(addr, &net_link_local_ip6, sizeof(struct in6_addr)) == 0 ||
	       memcmp(addr, &net_ip6, sizeof(struct in6_addr)) == 0;
}

void ip6_make_eui(unsigned char eui[8], unsigned char const enetaddr[6])
{
	memcpy(eui, enetaddr, 3);
	memcpy(&eui[5], &enetaddr[3], 3);
	eui[3] = 0xFF;
	eui[4] = 0xFE;
	eui[0] ^= 2;		/* "u" bit set to indicate global scope */
}

void ip6_make_lladdr(struct in6_addr *lladr, unsigned char const enetaddr[6])
{
	uchar eui[8];

	memset(lladr, 0, sizeof(struct in6_addr));
	lladr->s6_addr16[0] = htons(IPV6_LINK_LOCAL_PREFIX);
	ip6_make_eui(eui, enetaddr);
	memcpy(&lladr->s6_addr[8], eui, 8);
}

/*
 * Given an IPv6 address generate an equivalent Solicited Node Multicast
 * Address (SNMA) as described in RFC2461.
 */
void ip6_make_snma(struct in6_addr *mcast_addr, struct in6_addr *ip6_addr)
{
	memset(mcast_addr, 0, sizeof(struct in6_addr));
	mcast_addr->s6_addr[0] = 0xff;
	mcast_addr->s6_addr[1] = IPV6_ADDRSCOPE_LINK;
	mcast_addr->s6_addr[11] = 0x01;
	mcast_addr->s6_addr[12] = 0xff;
	mcast_addr->s6_addr[13] = ip6_addr->s6_addr[13];
	mcast_addr->s6_addr[14] = ip6_addr->s6_addr[14];
	mcast_addr->s6_addr[15] = ip6_addr->s6_addr[15];
}

/*
 * Given an IPv6 address generate the multicast MAC address that corresponds to
 * it.
 */
void
ip6_make_mult_ethdstaddr(unsigned char enetaddr[6], struct in6_addr *mcast_addr)
{
	enetaddr[0] = 0x33;
	enetaddr[1] = 0x33;
	memcpy(&enetaddr[2], &mcast_addr->s6_addr[12], 4);
}

int
ip6_addr_in_subnet(struct in6_addr *our_addr, struct in6_addr *neigh_addr,
		   __u32 plen)
{
	__be32 *addr_dwords;
	__be32 *neigh_dwords;

	addr_dwords = our_addr->s6_addr32;
	neigh_dwords = neigh_addr->s6_addr32;

	while (plen > 32) {
		if (*addr_dwords++ != *neigh_dwords++)
			return 0;

		plen -= 32;
	}

	/* Check any remaining bits. */
	if (plen > 0) {
		if ((*addr_dwords >> (32 - plen)) !=
		    (*neigh_dwords >> (32 - plen))) {
			return 0;
		}
	}

	return 1;
}

static inline unsigned int csum_fold(unsigned int sum)
{
	sum = (sum & 0xffff) + (sum >> 16);
	sum = (sum & 0xffff) + (sum >> 16);

	return ~sum;
}

static __u32 csum_do_csum(const __u8 *buff, int len)
{
	int odd, count;
	unsigned long result = 0;

	if (len <= 0)
		goto out;
	odd = 1 & (unsigned long)buff;
	if (odd) {
		result = *buff;
		len--;
		buff++;
	}
	count = len >> 1;	/* nr of 16-bit words.. */
	if (count) {
		if (2 & (unsigned long)buff) {
			result += *(unsigned short *)buff;
			count--;
			len -= 2;
			buff += 2;
		}
		count >>= 1;	/* nr of 32-bit words.. */
		if (count) {
			unsigned long carry = 0;
			do {
				unsigned long w = *(unsigned long *)buff;
				count--;
				buff += 4;
				result += carry;
				result += w;
				carry = (w > result);
			} while (count);
			result += carry;
			result = (result & 0xffff) + (result >> 16);
		}
		if (len & 2) {
			result += *(unsigned short *)buff;
			buff += 2;
		}
	}
	if (len & 1)
		result += (*buff << 8);
	result = ~csum_fold(result);
	if (odd)
		result = ((result >> 8) & 0xff) | ((result & 0xff) << 8);
out:
	return result;
}

unsigned int csum_partial(const unsigned char *buff, int len, unsigned int sum)
{
	unsigned int result = csum_do_csum(buff, len);

	/* add in old sum, and carry.. */
	result += sum;
	/* 16+c bits -> 16 bits */
	result = (result & 0xffff) + (result >> 16);
	return result;
}

/*
 * Compute checksum of IPv6 "psuedo-header" per RFC2460 section 8.1
 */
unsigned short int
csum_ipv6_magic(struct in6_addr *saddr, struct in6_addr *daddr,
		__u16 len, unsigned short proto, unsigned int csum)
{
	int i;
	int carry;
	__u32 ulen;
	__u32 uproto;
	unsigned int finalsum;

	/* The IPv6 header may not be 4-byte aligned,
	 * so make a safe copy of the source and destination
	 * addresses for the checksum. */
	struct in6_addr safe_saddr;
	struct in6_addr safe_daddr;

	memcpy(&safe_saddr, saddr, sizeof(safe_saddr));
	memcpy(&safe_daddr, daddr, sizeof(safe_daddr));

	for (i = 0; i < 4; i++) {
		csum += safe_saddr.s6_addr32[i];
		carry = (csum < safe_saddr.s6_addr32[i]);
		csum += carry;

		csum += safe_daddr.s6_addr32[i];
		carry = (csum < safe_daddr.s6_addr32[i]);
		csum += carry;
	}

	ulen = htonl((__u32)len);
	csum += ulen;
	carry = (csum < ulen);
	csum += carry;

	uproto = htonl(proto);
	csum += uproto;
	carry = (csum < uproto);
	csum += carry;

	finalsum = csum_fold(csum);
	if ((finalsum & 0xffff) == 0x0000)
		return 0xffff;
	else if ((finalsum & 0xffff) == 0xffff)
		return 0x0000;
	else
		return finalsum;
}

int
ip6_add_hdr(uchar *xip, struct in6_addr *src, struct in6_addr *dest,
	    int nextheader, int hoplimit, int payload_len)
{
	struct ip6_hdr *ip6 = (struct ip6_hdr *)xip;

	ip6->version = 6;
	ip6->priority = 0;
	ip6->flow_lbl[0] = 0;
	ip6->flow_lbl[1] = 0;
	ip6->flow_lbl[2] = 0;
	ip6->payload_len = htons(payload_len);
	ip6->nexthdr = nextheader;
	ip6->hop_limit = hoplimit;
	net_copy_ip6(&ip6->saddr, src);
	net_copy_ip6(&ip6->daddr, dest);

	return sizeof(struct ip6_hdr);
}

void net_ip6_handler(struct ethernet_hdr *et, struct ip6_hdr *ip6, int len)
{
	struct in_addr zero_ip = {.s_addr = 0 };
	struct icmp6hdr *icmp;
	struct udp_hdr *udp;
	__u16 csum;
	__u16 hlen;

	if (len < IP6_HDR_SIZE)
		return;

	if (ip6->version != 6)
		return;

	switch (ip6->nexthdr) {
	case IPPROTO_ICMPV6:
		icmp = (struct icmp6hdr *)(((uchar *)ip6) + IP6_HDR_SIZE);
		csum = icmp->icmp6_cksum;
		hlen = ntohs(ip6->payload_len);
		icmp->icmp6_cksum = 0;
		/* checksum */
		icmp->icmp6_cksum = csum_ipv6_magic(&ip6->saddr, &ip6->daddr,
						    hlen, IPPROTO_ICMPV6,
						    csum_partial((__u8 *)icmp, hlen, 0));
		if (icmp->icmp6_cksum != csum)
			return;

		switch (icmp->icmp6_type) {
		case IPV6_NDISC_NEIGHBOUR_SOLICITATION:
		case IPV6_NDISC_NEIGHBOUR_ADVERTISEMENT:
			ndisc_receive(et, ip6, len);
			break;

		default:
			return;
			break;
		}
		break;

	default:
		return;
		break;
	}
}
