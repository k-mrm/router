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
	unsigned short type;
	uint8_t buf[4096];
	uint8_t *packet;
	ssize_t nbytes;

	nbytes = recvpacket(dev, buf, 4096);
	if (nbytes < 0)
		return -1;

	eth = (struct ether_header *)buf;
	type = ntohs(eth->ether_type);
	packet = buf + ETHER_HEADER_SIZE;
	nbytes -= ETHER_HEADER_SIZE;

	switch (type) {
	case ETHER_TYPE_IPV4:
		return recvipv4(dev, packet, nbytes);

	case ETHER_TYPE_IPV6:
		return -1;
		// return recvipv6(dev, packet, nbytes);

	case ETHER_TYPE_ARP:
		return recvarp(dev, packet, nbytes);

	default:
		printf ("unknown ethernet type: %x\n", type);
		return -1;
	}
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
