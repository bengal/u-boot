/*
 * Simple IPv6 network layer implementation.
 *
 * Based and/or adapted from the IPv4 network layer in net.[hc]
 *
 * (C) Copyright 2013 Allied Telesis Labs NZ
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <environment.h>
#include <malloc.h>
#include <net.h>
#include <net6.h>
#include "ndisc.h"

/* Our gateway's IPv6 address */
struct in6_addr net_gateway6 = ZERO_IPV6_ADDR;
/* Our IPv6 addr (0 = unknown) */
struct in6_addr net_ip6 = ZERO_IPV6_ADDR;
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
