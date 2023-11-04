#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include "net.h"

int
checksum()
{
	;
}

int
recvipv4(NETDEV *dev, uint8_t *packet, size_t nbytes)
{
	return -1;
}
