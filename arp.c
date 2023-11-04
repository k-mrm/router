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

#include "net.h"

int
recvarp(NETDEV *dev, uint8_t *packet, size_t nbytes)
{
	struct arphdr *arp;

	arp = (struct arphdr *)packet;

	return -1;
}
