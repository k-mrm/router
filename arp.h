#ifndef __ROUTER_ARP_H
#define __ROUTER_ARP_H

#include <sys/types.h>
#include <netinet/if_ether.h>
#include "time.h"
#include "net.h"
#include "types.h"

typedef struct ARP ARP;
typedef struct ARPCACHE ARPCACHE;

struct ARP {
	unsigned short int hrd;
	unsigned short int pro;
	unsigned char hln;
	unsigned char pln;
	unsigned short int op;
	unsigned char sha[ETH_ALEN];
	unsigned int sip;
	unsigned char tha[ETH_ALEN];
	unsigned int tip;
} __packed;

struct ARPCACHE {
	NETDEV *dev;
	IP ipaddr;	// IPv4
	uchar hwaddr[6];
	time_t time;
	ARPCACHE *next;
};

#endif
