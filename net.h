#ifndef __ROUTER_NET_H
#define __ROUTER_NET_H

#include <stdbool.h>
#include <net/if.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/if_ether.h>
#include <linux/if_packet.h>
#include "types.h"

struct NAPT;

typedef struct NAPT		NAPT;
typedef struct NETDEV		NETDEV;
typedef struct SKBUF		SKBUF;
typedef struct IP_PSEUDO	IP_PSEUDO;

extern bool term;

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

	NAPT *napt;
};

extern NETDEV *netdev[16];
extern int ndev;

void bug(const char *fmt);
void sigtrap(void);

NETDEV *opennetdev(const char *name, bool promisc);
void pnetdev(NETDEV *dev);

NETDEV *devbysubnet(IP subnet);
NETDEV *devbyipaddr(IP addr);

char *ipv4addrfmt(IP ipaddr, char *str);
char *hwaddrfmt(uchar *mac, char *str);

// socket buffer
struct SKBUF {
	void *head;
	void *data;
	void *tail;
	size_t size;
	uint ref;	// refcount

	// Ether header tracking
	struct ether_header *eth;
	// IP header tracking
	struct iphdr *iphdr;
	ushort iphdrsz;
	uchar ipproto;
};

// pseudo IP header
struct IP_PSEUDO {
	uint srcip;
	uint dstip;
	uchar reserved;
	uchar proto;
	ushort len;
};

SKBUF *skalloc(size_t size);
void skfree(SKBUF *buf);
ushort ipchecksum(SKBUF *buf);

void skref(SKBUF *buf);

void *skpush(SKBUF *buf, size_t size);
void *skpull(SKBUF *buf, size_t size);
void *skpulleth(SKBUF *buf);
void *skpullip(SKBUF *buf);

void skcopy(SKBUF *buf, uchar *src, size_t n);

ushort checksum(uchar *buf, size_t n);
ushort checksum2(uchar *buf1, size_t n1, uchar *buf2, size_t n2);

ssize_t sendpacket(NETDEV *dev, SKBUF *buf);
ssize_t recvpacket(NETDEV *dev, uchar *buf, size_t nbuf);

#endif
