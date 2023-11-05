#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <net/ethernet.h>
#include <net/if.h>
#include <netinet/in.h>
#include <netinet/if_ether.h>
#include <linux/if_packet.h>

#include "net.h"
#include "ether.h"

int
recvether(NETDEV *dev)
{
	struct ether_header *eth;
	ushort type;
	uchar b[4096];
	ssize_t nbytes;
	SKBUF *buf;
	int rc;

	nbytes = recvpacket(dev, b, 4096);
	if (nbytes < 0) {
		return -1;
	}

	buf = skalloc(nbytes);
	if (!buf) {
		return -1;
	}
	skcopy(buf, b, nbytes);

	eth = skpulleth(buf);
	if (!eth) {
		rc = -1;
		goto err;
	}

	type = ntohs(eth->ether_type);

	switch (type) {
	case ETHER_TYPE_IPV4:
		rc = recvipv4(dev, buf);
		break;

	case ETHER_TYPE_IPV6:
		goto err;
		// return recvipv6(dev, buf);

	case ETHER_TYPE_ARP:
		rc = recvarp(dev, buf);
		break;

	default:
		printf ("unknown ethernet type: %x\n", type);
		goto err;
	}

err:
	skfree(buf);
	return rc;
}

ssize_t
sendether(NETDEV *dev, uchar *daddr, SKBUF *buf, ushort type)
{
	struct ether_header *eth;
	uchar *saddr;

	eth = skpush(buf, sizeof *eth);
	if (!eth) {
		return -1;
	}

	saddr = dev->hwaddr;

	ethaddrcpy(eth->ether_dhost, daddr);
	ethaddrcpy(eth->ether_shost, saddr);
	eth->ether_type = htons(type);

	sendpacket(dev, buf);
}

void
ethaddrcpy(uchar *dst, uchar *src)
{
	memcpy(dst, src, ETHER_ADDR_LEN);
}

void
ethzero(uchar *addr)
{
	memset(addr, 0, 6);
}
