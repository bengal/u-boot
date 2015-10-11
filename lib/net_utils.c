// SPDX-License-Identifier: GPL-2.0+
/*
 * Generic network code. Moved from net.c
 *
 * Copyright 1994 - 2000 Neil Russell.
 * Copyright 2000 Roland Borde
 * Copyright 2000 Paolo Scaffardi
 * Copyright 2000-2002 Wolfgang Denk, wd@denx.de
 * Copyright 2009 Dirk Behme, dirk.behme@googlemail.com
 */

#include <common.h>
#include <net6.h>
#include <linux/ctype.h>

struct in_addr string_to_ip(const char *s)
{
	struct in_addr addr;
	char *e;
	int i;

	addr.s_addr = 0;
	if (s == NULL)
		return addr;

	for (addr.s_addr = 0, i = 0; i < 4; ++i) {
		ulong val = s ? simple_strtoul(s, &e, 10) : 0;
		if (val > 255) {
			addr.s_addr = 0;
			return addr;
		}
		if (i != 3 && *e != '.') {
			addr.s_addr = 0;
			return addr;
		}
		addr.s_addr <<= 8;
		addr.s_addr |= (val & 0xFF);
		if (s) {
			s = (*e) ? e+1 : e;
		}
	}

	addr.s_addr = htonl(addr.s_addr);
	return addr;
}

void string_to_enetaddr(const char *addr, uint8_t *enetaddr)
{
	char *end;
	int i;

	if (!enetaddr)
		return;

	for (i = 0; i < 6; ++i) {
		enetaddr[i] = addr ? simple_strtoul(addr, &end, 16) : 0;
		if (addr)
			addr = (*end) ? end + 1 : end;
	}
}

uint compute_ip_checksum(const void *vptr, uint nbytes)
{
	int sum, oddbyte;
	const unsigned short *ptr = vptr;

	sum = 0;
	while (nbytes > 1) {
		sum += *ptr++;
		nbytes -= 2;
	}
	if (nbytes == 1) {
		oddbyte = 0;
		((u8 *)&oddbyte)[0] = *(u8 *)ptr;
		((u8 *)&oddbyte)[1] = 0;
		sum += oddbyte;
	}
	sum = (sum >> 16) + (sum & 0xffff);
	sum += (sum >> 16);
	sum = ~sum & 0xffff;

	return sum;
}

uint add_ip_checksums(uint offset, uint sum, uint new)
{
	ulong checksum;

	sum = ~sum & 0xffff;
	new = ~new & 0xffff;
	if (offset & 1) {
		/*
		 * byte-swap the sum if it came from an odd offset; since the
		 * computation is endian-independent this works.
		 */
		new = ((new >> 8) & 0xff) | ((new << 8) & 0xff00);
	}
	checksum = sum + new;
	if (checksum > 0xffff)
		checksum -= 0xffff;

	return (~checksum) & 0xffff;
}

int ip_checksum_ok(const void *addr, uint nbytes)
{
	return !(compute_ip_checksum(addr, nbytes) & 0xfffe);
}

#ifdef CONFIG_NET6
/**
 * Parses an struct in6_addr from the given string. IPv6 address parsing is a bit
 * more complicated than v4 due to the flexible format and some of the special
 * cases (e.g. v4 mapped).
 *
 * Examples of valid strings:
 *   2001:db8::0:1234:1
 *   2001:0db8:0000:0000:0000:0000:1234:0001
 *   ::1
 *   ::ffff:192.168.1.1
 *
 * Examples of invalid strings
 *   2001:db8::0::0          (:: can only appear once)
 *   2001:db8:192.168.1.1::1 (v4 part can only appear at the end)
 *   192.168.1.1             (we don't implicity map v4)
 */
int string_to_ip6(const char *strpt, struct in6_addr *addrpt)
{
	int colon_count = 0;
	int found_double_colon = 0;
	int xstart = 0;		/* first zero (double colon) */
	int len = 7;		/* num words the double colon represents */
	int i;
	const char *s = strpt;
	struct in_addr zero_ip = {.s_addr = 0};

	if (strpt == NULL)
		return -1;

	/* First pass, verify the syntax and locate the double colon */
	for (;;) {
		while (isxdigit((int)*s))
			s++;
		if (*s == '\0')
			break;
		if (*s != ':') {
			if (*s == '.' && len >= 2) {
				struct in_addr v4;
				while (s != strpt && *(s - 1) != ':')
					--s;
				v4 = string_to_ip(s);
				if (memcmp(&zero_ip, &v4,
					   sizeof(struct in_addr) != 0)) {
					len -= 2;
					break;
				}
			}
			/* This could be a valid address */
			break;
		}
		if (s == strpt) {
			/* The address begins with a colon */
			if (*++s != ':')
				/* Must start with a double colon or a number */
				goto out_err;
		} else {
			s++;
			if (found_double_colon)
				len--;
			else
				xstart++;
		}

		if (*s == ':') {
			if (found_double_colon)
				/* Two double colons are not allowed */
				goto out_err;
			found_double_colon = 1;
			len -= xstart;
			s++;
		}

		if (++colon_count == 7)
			/* Found all colons */
			break;
	}

	if (colon_count == 0)
		goto out_err;
	if (*--s == ':')
		len++;

	/* Second pass, read the address */
	s = strpt;
	for (i = 0; i < 8; i++) {
		int val = 0;
		char *end;

		if (found_double_colon && i >= xstart && i < xstart + len) {
			addrpt->s6_addr16[i] = 0;
			continue;
		}
		while (*s == ':')
			s++;

		if (i == 6 && isdigit((int)*s)) {
			struct in_addr v4 = string_to_ip(s);
			if (memcmp(&zero_ip, &v4,
				   sizeof(struct in_addr)) != 0) {
				/* Ending with :IPv4-address */
				addrpt->s6_addr32[3] = v4.s_addr;
				break;
			}
		}

		val = simple_strtoul(s, &end, 16);
		if (*end != '\0' && *end != ':')
			goto out_err;
		addrpt->s6_addr16[i] = htons(val);
		s = end;
	}
	return 0;

out_err:
	return -1;
}
#endif
