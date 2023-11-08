#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <poll.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <net/ethernet.h>
#include <net/if.h>
#include <netinet/in.h>
#include <netinet/if_ether.h>
#include <linux/if_packet.h>

#include "types.h"
#include "net.h"
#include "route.h"
#include "ipv4.h"
#include "ether.h"
#include "napt.h"

NETDEV *netdev[16];
int ndev = 0;
bool term = false;

static IP nexthop = IPADDRESS(192,168,0,1);

static void
netdevconfig()
{
	NETDEV *dev;

	for (int i = 0; i < ndev; i++) {
		dev = netdev[i];
		routeconnected(dev->ipv4.subnet, dev->ipv4.netmask, dev);
	}

	// default gw
	routenexthop(IPADDRESS(0,0,0,0), IPADDRESS(0,0,0,0), nexthop);

	// Add NAT: 192.168.1.0/24 -> 192.168.0.24
	confignat(IPADDRESS(192,168,1,0), IPADDRESS(255,255,255,0),
	          IPADDRESS(192,168,0,24));
}

static int
disableipforward()
{
	FILE *fp;

	if (!(fp = fopen("/proc/sys/net/ipv4/ip_forward", "w"))) {
		return -1;
	}
	fputs("0", fp);

	fclose(fp);

	return 0;
}

static int
routercore(struct pollfd *fd)
{
	int nready;
	NETDEV *dev;

	nready = poll(fd, ndev, -1);
	if (nready < 0) {
		return -1;
	}

	for (int i = 0; i < ndev && nready; i++) {
		if (fd[i].revents & (POLLIN | POLLERR)) {
			dev = netdev[i];
			recvether(dev);
			nready--;
		}
	}

	return 0;
}

static int
router()
{
	struct pollfd fd[16];

	for (int i = 0; i < ndev; i++) {
		struct pollfd *f = &fd[i];
		f->fd = netdev[i]->soc;
		f->events = POLLIN | POLLERR;
	}

	while (!term) {
		routercore(fd);
	}

	return 0;
}

int
main(int argc, char **argv)
{
	NETDEV *dev;
	int rc;

	for (int i = 1; i < argc; i++) {
		dev = opennetdev(argv[i], 1);
		if (!dev) {
			printf("cannot open %s\n", argv[i]);
			continue;
		}

		netdev[ndev++] = dev;
		pnetdev(dev);
	}

	if (ndev == 0) {
		bug("no device, aborted");
		return -1;
	}

	sigtrap();
	netdevconfig();
	disableipforward();

	rc = router();

	return rc;
}
