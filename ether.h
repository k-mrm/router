#ifndef __ROUTER_ETHER_H
#define __ROUTER_ETHER_H

#include <netinet/if_ether.h>
#include <linux/if_packet.h>
#include "types.h"
#include "net.h"

#define ETHER_TYPE_IPV4		0x0800
#define ETHER_TYPE_ARP 		0x0806
#define ETHER_TYPE_IPV6		0x86dd

#define ETHER_HEADER_SIZE 	14

int recvether(NETDEV *dev);
ssize_t sendether(NETDEV *dev, uchar *daddr, SKBUF *buf, ushort type);
void ethaddrcpy(uchar *dst, uchar *src);

#endif
