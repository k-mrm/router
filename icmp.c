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
sendicmp(NETDEV *dev, IP dst, uchar type, SKBUF *buf)
{
	ICMPHDR *icmp;

	icmp = skpush(buf, sizeof(ICMPHDR));

	icmp->type = type;
	icmp->code = 0;
	icmp->checksum = htonl(checksum((uchar *)icmp, buf->size));

	return sendipv4(dev, dst, buf, IPPROTO_ICMP);
}

int
icmptimeexceeded(NETDEV *dev, IP dst)
{
	SKBUF *buf;

	buf = skalloc(sizeof(ICMP_TIME_EXCEEDED));
	if (!buf) {
		return -1;
	}

}

int
icmpunreachable(NETDEV *dev)
{
	;
}

int
icmpechoreply(NETDEV *dev, IP dst, SKBUF *echo)
{
	return sendicmp(dev, dst, ICMP_TYPE_ECHO_RESPONSE, echo);
}

int
recvicmp(NETDEV *dev, IP src, SKBUF *buf)
{
	;
}
