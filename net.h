#ifndef __ROUTER_NET_H
#define __ROUTER_NET_H

#include <stdbool.h>
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

NETDEV *opennetdev(const char *name, bool promisc);

// socket buffer
struct SKBUF {
	void *head;
	void *data;
	void *tail;
	size_t size;
};

SKBUF *skalloc(size_t size);
void skfree(SKBUF *buf);
void *skpush(SKBUF *buf, size_t size);
void skcopy(SKBUF *buf, uchar *src, size_t n);

#endif
