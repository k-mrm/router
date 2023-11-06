#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include "types.h"
#include "net.h"
#include "ipv4.h"
#include "icmp.h"

static int
sendicmp(NETDEV *dev, IP dst, uchar type, uchar code, SKBUF *buf)
{
	ICMPHDR *icmp;
	IP src;

	icmp = skpush(buf, sizeof(ICMPHDR));
	if (!icmp) {
		return -1;
	}

	icmp->type = type;
	icmp->code = code;
	icmp->checksum = htonl(checksum((uchar *)icmp, buf->size));

	src = dev->ipv4.addr;

	return sendipv4(dev, dst, src, buf, IPPROTO_ICMP);
}

int
icmptimeexceeded(NETDEV *dev, IP dst, SKBUF *pkt)
{
	ICMP_TIME_EXCEEDED *te;
	SKBUF *buf;

	buf = skalloc(sizeof(ICMP_TIME_EXCEEDED) + pkt->size);
	if (!buf) {
		return -1;
	}

	te = buf->data;

	memcpy(te->data, pkt->data, pkt->size);

	return sendicmp(dev, dst, ICMP_TYPE_TIME_EXCEEDED, 0, buf);
}

int
icmpunreachable(NETDEV *dev, IP dst, SKBUF *pkt)
{
	ICMP_UNREACHABLE *un;
	SKBUF *buf;

	buf = skalloc(sizeof(ICMP_UNREACHABLE) + pkt->size);
	if (!buf) {
		return -1;
	}

	un = buf->data;

	memcpy(un->data, pkt->data, pkt->size);

	return sendicmp(dev, dst, ICMP_TYPE_DESTINATION_UNREACHABLE, 0, buf);
}

int
icmpechoreply(NETDEV *dev, IP dst, SKBUF *echo)
{
	return sendicmp(dev, dst, ICMP_TYPE_ECHO_RESPONSE, 0, echo);
}

int
recvicmp(NETDEV *dev, IP src, SKBUF *buf)
{
	ICMPHDR *hdr;

	hdr = skpull(buf, sizeof(ICMPHDR));
	if (!hdr) {
		return -1;
	}

	switch (hdr->type) {
	case ICMP_TYPE_ECHO_REQUEST:
		return icmpechoreply(dev, src, buf);

	default:
		break;
	}

	return 0;
}
