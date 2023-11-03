#ifndef __ROUTER_NET_H
#define __ROUTER_NET_H

#include <stdbool.h>

typedef struct NETIF NETIF;
typedef struct ARPTABLE ARPTABLE;

struct NETDEV {
	int soc;
	const char *name;
	unsigned char hwaddr[6];
};

NETDEV *opennetdev(const char *name, bool promisc);

#endif
