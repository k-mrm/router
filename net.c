#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <net/ethernet.h>
#include <net/if.h>
#include <netinet/in.h>
#include <netinet/if_ether.h>
#include <linux/if_packet.h>

#include "net.h"

NETDEV *
opennetdev(const char *name, bool promisc)
{
	struct ifreq ifr;
	struct sockaddr_ll sa;
	NETDEV *dev;

	dev = malloc(sizeof *dev);
	if (!dev)
		return NULL;

	dev->name = name;

	dev->soc = socket(PF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
	if (dev->soc < 0)
		goto err;

	strncpy(ifr.ifr_name, name, sizeof(ifr.ifr_name) - 1);

	if (ioctl(dev->soc, SIOCGIFINDEX, &ifr) < 0) {
		perror("SIOCGIFINDEX");
		goto err;
	}

	sa.sll_family = PF_PACKET;
	sa.sll_protocol = htons(ETH_P_ALL);
	sa.sll_ifindex = ifr.ifr_ifindex;

	if (bind(dev->soc, (struct sockaddr *)&sa, sizeof sa) < 0) {
		goto err;
	}

	if (promisc) {
		if (ioctl(dev->soc, SIOCGIFFLAGS, &ifr) < 0) {
			perror("SIOCGIFFLAGS");
			goto err;
		}

		ifr.ifr_flags |= IFF_PROMISC;

		if(ioctl(dev->soc, SIOCSIFFLAGS, &ifr) < 0) {
			perror("SIOCSIFFLAGS");
			goto err;
		}
	}

	if (ioctl(dev->soc, SIOCGIFHWADDR, &ifr) < 0) {
		perror("SIOCGIFHWADDR");
		goto err;
	}
	ethaddrcpy(dev->hwaddr, ifr.ifr_hwaddr.sa_data);

	return dev;

err:
	if (dev->soc >= 0)
		close(dev->soc);
	free(dev);

	return NULL;
}

ssize_t
sendpacket(NETDEV *dev, unsigned char *buf, size_t nbuf)
{
	return write(dev->soc, buf, nbuf);
}

ssize_t
recvpacket(NETDEV *dev, unsigned char *buf, size_t nbuf)
{
	return read(dev->soc, buf, nbuf);
}
