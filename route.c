#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "types.h"
#include "net.h"
#include "route.h"

static ROUTE routetable[128];
static int nrt = 0;

static int
rtadd(IP subnet, IP mask, ROUTETYPE type, IP nexthop, NETDEV *con)
{
	ROUTE *rt;

	rt = &routetable[nrt++];

	if (nrt == 128) {
		bug("too many table");
		return -1;
	}

	rt->type = type;
	rt->subnet = subnet;
	rt->mask = mask;

	if (type == NEXTHOP) {
		rt->nexthop = nexthop;
	} else if (type == CONNECTED) {
		rt->connect = con;
	} else {
		bug("unreachable");
	}

	return 0;
}

static bool
insubnet(IP addr, IP subnet, IP mask)
{
	IP sub;

	sub = addr & mask;

	return sub == subnet;
}

int
rtaddconnected(IP subnet, IP mask, NETDEV *con)
{
	return rtadd(subnet, mask, CONNECTED, 0, con);
}

int
rtaddnexthop(IP subnet, IP mask, IP nexthop)
{
	return rtadd(subnet, mask, NEXTHOP, nexthop, NULL);
}

ROUTE *
rtsearch(IP ipaddr)
{
	ROUTE *match = NULL;
	ROUTE *rt;
	uint longest = 0;

	for (rt = routetable; rt < &routetable[nrt]; rt++) {
		// longest match
		if (insubnet(ipaddr, rt->subnet, rt->mask)) {
			if (rt->mask >= longest) {
				longest = rt->mask;
				match = rt;
			}
		}
	}

	return match;
}
