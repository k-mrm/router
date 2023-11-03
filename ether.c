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

ssize_t
recvether(NETDEV *dev)
{
	;
}

ssize_t
sendether(NETDEV *dev, uint8_t *buf, size_t nbuf)
{
	;
}

void
ethaddrcpy(uint8_t *src, uint8_t *dst)
{
	memcpy(src, dst, 6);
}
