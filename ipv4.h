#ifndef __ROUTER_IPV4_H
#define __ROUTER_IPV4_H

#include "types.h"
#include "net.h"
#include <netinet/ip.h>

#define IPADDRESS(a, b, c, d)	(a * 0x1000000u + b * 0x10000 + c * 0x100 + d)

int sendipv4(NETDEV *dev, IP dst, IP src, SKBUF *buf, uchar proto);
int recvipv4(NETDEV *dev, SKBUF *buf);

#endif
