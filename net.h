#ifndef __ROUTER_NET_H
#define __ROUTER_NET_H

#include <stdbool.h>
#include <net/if.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/if_ether.h>
#include <linux/if_packet.h>
#include "types.h"

typedef struct NETDEV NETDEV;
typedef struct SKBUF SKBUF;

struct NETDEV {
	int soc;
	const char *name;
	uchar hwaddr[6];
	struct {
		IP addr;
		IP netmask;
		IP subnet;
	} ipv4;
	struct {
		// ?
	} ipv6;
};

extern NETDEV *netdev[16];
extern int ndev;

NETDEV *opennetdev(const char *name, bool promisc);

// socket buffer
struct SKBUF {
	void *head;
	void *data;
	void *tail;
	size_t size;

	// header tracking
	struct ether_header *eth;
	struct iphdr *iphdr;
	ushort iphdrsz;
};

SKBUF *skalloc(size_t size);
void skfree(SKBUF *buf);

void *skpush(SKBUF *buf, size_t size);
void *skpull(SKBUF *buf, size_t size);
void *skpulleth(SKBUF *buf);
void *skpullip(SKBUF *buf);

void skcopy(SKBUF *buf, uchar *src, size_t n);

ushort checksum(uchar *buf, size_t n);

ssize_t sendpacket(NETDEV *dev, SKBUF *buf);
ssize_t recvpacket(NETDEV *dev, uchar *buf, size_t nbuf);

#endif
