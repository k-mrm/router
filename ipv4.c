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
#include "icmp.h"
#include "napt.h"

static void
handlemypkt(NETDEV *dev, IP src, SKBUF *buf)
{
	uchar proto;

	// L4
	proto = buf->ipproto;

	switch (proto) {
	case IPPROTO_ICMP:
		recvicmp(dev, src, buf);
		break;
	default:
		break;
	}
}

static int
mypacket(SKBUF *buf, IP dst, IP src, bool *napted)
{
	NETDEV *dev;
	int rc;
	char dbg[16], dbg2[16];

	*napted = false;

	// NAPT ?
	for (int i = 0; i < ndev; i++) {
		dev = netdev[i];
		if (dev->napt && dev->napt->out == dst) {
			printf("IN NAPT!1 %s: dst=%s src=%s\n", dev->name, ipv4addrfmt(dst, dbg), ipv4addrfmt(src, dbg2));
			rc = napt(dev->napt, INCOMING, buf);
			if (rc == 0) {
				*napted = true;
				return 0;
			}
		}
	}

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
schedule(NETDEV *dev, IP dst, SKBUF *buf)
{
	return -1;
}

static int
routeipv4(SKBUF *buf)
{
	NETDEV *dev;
	ROUTE *rt;
	ARPCACHE *arp;
	char dbg[16], dbg2[16], dbg3[32];
	struct iphdr *iphdr = buf->data;
	IP dst;
	ushort csum;

	dst = ntohl(iphdr->daddr);
	csum = iphdr->check;

	rt = rtsearch(dst);

	if (!rt) {
		// drop
		bug("?");
		return -1;
	}

	if (rt->type == CONNECTED) {
		dev = rt->connect;
		arp = searcharptable(dst);

		printf("%s route to %s\n", ipv4addrfmt(dst, dbg), dev->name);

		if (!arp) {
			// Who is dst?
			sendarpreq(dev, dst);

			return schedule(dev, dst, buf);
		}
		else {
			printf("directconnect: --> %s\n", hwaddrfmt(arp->hwaddr, dbg3));
			return sendether(dev, arp->hwaddr, buf, ETHER_TYPE_IPV4);
		}
	}
	else if (rt->type == NEXTHOP) {
		IP nexthop = rt->nexthop;

		printf("%s route to nexthop(%s)\n", ipv4addrfmt(dst, dbg), ipv4addrfmt(nexthop, dbg2));

		arp = searcharptable(nexthop);

		if (!arp) {
			ROUTE *rnexthop;

			// Who is nexthop?
			rnexthop = rtsearch(nexthop);

			if (!rnexthop || rnexthop->type != CONNECTED) {
				// no nexthop ;;
				return -1;
			}

			dev = rnexthop->connect;

			sendarpreq(dev, nexthop);

			return schedule(dev, nexthop, buf);
		}
		else {
			return sendether(arp->dev, arp->hwaddr, buf, ETHER_TYPE_IPV4);
		}
	}
	else {
		bug("unreachable");
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
	char dbg[16];
	bool napted;

	iphdr = skpullip(buf);
	if (!iphdr) {
		bug("");
		return -1;
	}

	src = ntohl(iphdr->saddr);
	dst = ntohl(iphdr->daddr);

	printf("%s RECV IP!: %s --> ", dev->name, ipv4addrfmt(src, dbg));
	printf("%s\n", ipv4addrfmt(dst, dbg));

	// check checksum
	sum = ipchecksum(buf);
	if (sum != 0 && sum != 0xffff) {
		printf("invalid checksum=%x\n", iphdr->check);
		return -1;
	}

	rc = mypacket(buf, dst, src, &napted);
	if (rc) {
		return 0;
	}
	if (napted) {
		goto sendip;
	}

	iphdr->ttl--;
	if (iphdr->ttl == 0) {
		printf("packet ttl0\n");
		icmptimeexceeded(dev, src, buf);
		return -1;
	}

	rc = napt(dev->napt, OUTGOING, buf);
	if (rc < 0) {
		// Drop
		printf("NAPT FAILED ToT\n");
		return -1;
	}

sendip:
	skpush(buf, buf->iphdrsz);
	skref(buf);

	printf("%s recv ip: from %s ", dev->name, ipv4addrfmt(ntohl(buf->iphdr->saddr), dbg));
	printf(" -> %s\n", ipv4addrfmt(ntohl(buf->iphdr->daddr), dbg));
	// routing
	routeipv4(buf);

	return 0;
}

int
sendipv4(IP dst, IP src, SKBUF *buf, uchar proto)
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
	ip->ttl = 32;
	ip->tos = 0;
	ip->frag_off = 0;
	ip->id = htons(id++);

	ip->tot_len = htons(total);

	ip->daddr = htonl(dst);
	ip->saddr = htonl(src);

	ip->check = 0;
	ip->check = checksum((uchar *)ip, sizeof *ip);

	return routeipv4(buf);
}
