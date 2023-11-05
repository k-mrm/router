#ifndef __ROUTER_IPV4_H
#define __ROUTER_IPV4_H

#include "types.h"
#include "net.h"
#include <netinet/ip.h>

int sendipv4(NETDEV *dev, IP dst, SKBUF *buf, uchar proto);
int recvipv4(NETDEV *dev, SKBUF *buf);

#endif
