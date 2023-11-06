#ifndef __ROUTER_ROUTE_H
#define __ROUTER_ROUTE_H

#include "types.h"
#include "net.h"

typedef enum ROUTETYPE ROUTETYPE;
typedef struct ROUTE ROUTE;

enum ROUTETYPE {
	NEXTHOP,
	CONNECTED,
};

struct ROUTE {
	ROUTETYPE type;
	IP subnet;
	IP mask;
	union {
		IP nexthop;
		NETDEV *connect;
	};
};

int rtaddconnected(IP subnet, IP mask, NETDEV *con);
int rtaddnexthop(IP subnet, IP mask, IP nexthop);
ROUTE *rtsearch(IP ipaddr);

#endif	// __ROUTER_ROUTE_H
