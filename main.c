#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <net/ethernet.h>
#include <net/if.h>
#include <netinet/in.h>
#include <netinet/if_ether.h>
#include <linux/if_packet.h>

static NETDEV *netdev[16];
static int ndev;
static bool term = false;

static int
routercore(struct pollfd *fd)
{
	int nready;
	ssize_t size;
	NETDEV *dev;

	nready = poll(fd, ndev, -1);
	if (nready < 0)
		return -1;

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
	int rc;

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
		if (!dev)
			return -1;

		netdev[ndev++] = dev;
	}

	rc = router();

	return rc;
}
