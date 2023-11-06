#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/if_ether.h>

#include "types.h"
#include "net.h"
#include "ether.h"
#include "route.h"
#include "arp.h"

static void
handlemypkt(NETDEV *dev, IP src, SKBUF *buf)
{
	struct iphdr *iphdr = buf->iphdr;
	uchar proto;

	if (!iphdr) {
		return;
	}

	proto = iphdr->protocol;

	switch (proto) {
	case IPPROTO_ICMP:
		recvicmp(dev, src, buf);
		break;
	default:
		break;
	}
}

static int
mypacket(SKBUF *buf, IP dst, IP src)
{
	NETDEV *dev;

	for (int i = 0; i < ndev; i++) {
		dev = netdev[i];
		if (dev->ipv4.addr == dst) {
			// This is my packet!
			handlemypkt(dev, src, buf);
			return 1;
		}
	}

	return 0;
}

static int
routeipv4(IP dst, SKBUF *buf)
{
	NETDEV *dev;
	ROUTE *rt;
	uchar *dstmac;
	ARPCACHE *arp;

	rt = rtsearch(dst);

	if (!rt) {
		// drop
		return -1;
	}

	if (rt->type == CONNECTED) {
		dev = rt->connect;
		arp = searcharptable(dst);

		if (!arp) {
			// Who is dst?
			sendarpreq(dev, dst);
		}

		return sendether(dev, arp->hwaddr, buf, ETHER_TYPE_IPV4);
	} else if (rt->type == NEXTHOP) {
		IP nexthop = rt->nexthop;

		arp = searcharptable(nexthop);

		if (!arp) {
			// Who is nexthop?
			ROUTE *rnexthop;

			rnexthop = rtsearch(nexthop);

			if (!rnexthop || rnexthop->type != CONNECTED) {
				// nexthop?
				return -1;
			}

			dev = rnexthop->connect;

			sendarpreq(dev, nexthop);
		}

		return sendether(arp->dev, arp->hwaddr, buf, ETHER_TYPE_IPV4);
	} else {
		return -1;
	}
}

int
recvipv4(NETDEV *dev, SKBUF *buf)
{
	struct iphdr *iphdr;
	ushort sum;
	IP src, dst;
	int rc;

	iphdr = skpullip(buf);
	if (!iphdr) {
		return -1;
	}

	if (iphdr->version != 4) {
		return -1;
	}

	src = ntohl(iphdr->saddr);
	dst = ntohl(iphdr->daddr);

	// check checksum
	sum = checksum((uchar *)iphdr, buf->iphdrsz);
	if (sum != 0 && sum != 0xffff) {
		return -1;
	}

	rc = mypacket(buf, dst, src);
	if (rc) {
		return 0;
	}

	iphdr->ttl--;
	if (iphdr->ttl == 0) {
		printf("packet ttl0\n");
		icmptimeexceeded(dev, src);
		return -1;
	}

	skpush(buf, buf->iphdrsz);

	// routing
	routeipv4(dst, buf);

	return 0;
}

int
sendipv4(NETDEV *dev, IP dst, IP src, SKBUF *buf, uchar proto)
{
	static ushort id = 0;
	struct iphdr *ip;
	ushort total = 0;

	ip = skpush(buf, sizeof *ip);
	if (!ip) {
		return -1;
	}

	total = buf->size;

	ip->version = 4;
	ip->ihl = sizeof(*ip) >> 2;
	ip->protocol = proto;
	ip->ttl = 64;
	ip->tos = 0;
	ip->frag_off = 0;
	ip->id = id++;

	ip->tot_len = htons(total);

	ip->daddr = htonl(dst);
	ip->saddr = htonl(src);

	ip->check = 0;
	ip->check = checksum((uchar *)ip, sizeof *ip);

	return routeipv4(dst, buf);
}
