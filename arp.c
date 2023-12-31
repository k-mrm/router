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

#include "types.h"
#include "net.h"
#include "ether.h"
#include "arp.h"

// Hash Table
static ARPCACHE *arptable[131];

static void
arptimestamp(ARPCACHE *cache)
{
	;
}

static void
addarptable(NETDEV *dev, uchar *hwaddr, IP ipaddr)
{
	ARPCACHE *cache;
	ARPCACHE *entry;
  	char ip[16] = {0};
	char mac[32] = {0};

	cache = malloc(sizeof *cache);
	if (!cache) {
		return;
	}

	ethaddrcpy(cache->hwaddr, hwaddr);
	cache->ipaddr = ipaddr;
	cache->dev = dev;
	arptimestamp(cache);

	entry = arptable[ipaddr % 131];
	cache->next = entry;
	arptable[ipaddr % 131] = cache;

  	ipv4addrfmt(cache->ipaddr, ip);
	hwaddrfmt(cache->hwaddr, mac);

	printf("new arpcache: %s: %s -> %s\n", cache->dev->name, ip, mac);
}

ARPCACHE *
searcharptable(IP ipaddr)
{
	ARPCACHE *entry;

	entry = arptable[ipaddr % 131];

	for (ARPCACHE *c = entry; c; c = c->next) {
		if (c->ipaddr == ipaddr) {
			arptimestamp(c);
			return c;
		}
	}

	return NULL;
}

static void
dumparptable()
{
	ARPCACHE *ent;

	printf("ARPTABLE: \n");
	for (int i = 0; i < 131; i++) {
  		char ip[16] = {0};
		char mac[32] = {0};

		ent = arptable[i];
		if (!ent) {
			continue;
		}

		for (ARPCACHE *c = ent; c; c = c->next) {
  			ipv4addrfmt(c->ipaddr, ip);
			hwaddrfmt(c->hwaddr, mac);

			printf("  %s: %s -> %s\n", c->dev->name, ip, mac);
		}
	}
}

int
sendarpreq(NETDEV *dev, IP tip)
{
	ARP arphdr = {0};
	SKBUF *buf;
	uchar *sha = dev->hwaddr;
	uchar bcast[6] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
	IP sip = dev->ipv4.addr;

	buf = skalloc(sizeof(ARP));
	if (!buf) {
		return -1;
	}

	arphdr.hrd = htons(ARPHRD_ETHER);
	arphdr.pro = htons(ETHER_TYPE_IPV4);
	arphdr.hln = ETH_ALEN;
	arphdr.pln = 4;
	arphdr.op = htons(ARPOP_REQUEST);

	ethaddrcpy(arphdr.sha, sha);
	arphdr.sip = htonl(sip);
	ethzero(arphdr.tha);
	arphdr.tip = htonl(tip);

	skcopy(buf, (uchar *)&arphdr, sizeof(ARP));

	return sendether(dev, bcast, buf, ETHER_TYPE_ARP);
}

static int
sendarpreply(NETDEV *dev, uchar *tha, IP tip, uchar *sha, IP sip)
{
	ARP arphdr;
	SKBUF *buf;

	buf = skalloc(sizeof(ARP));
	if (!buf) {
		return -1;
	}

	arphdr.hrd = htons(ARPHRD_ETHER);
	arphdr.pro = htons(ETHER_TYPE_IPV4);
	arphdr.hln = ETH_ALEN;
	arphdr.pln = 4;
	arphdr.op = htons(ARPOP_REPLY);

	ethaddrcpy(arphdr.sha, sha);
	arphdr.sip = htonl(sip);
	ethaddrcpy(arphdr.tha, tha);
	arphdr.tip = htonl(tip);

	skcopy(buf, (uchar *)&arphdr, sizeof(ARP));

	return sendether(dev, tha, buf, ETHER_TYPE_ARP);
}

static int
recvarpreq(NETDEV *dev, ARP *arp)
{
	IP tip;
	uchar *target;

	tip = ntohl(arp->tip);

	if (dev->ipv4.addr == tip) {
		// It's me
		target = dev->hwaddr;
		sendarpreply(dev, arp->sha, arp->sip, target, tip);
	}
	
	// sender information
	addarptable(dev, arp->sha, ntohl(arp->sip));

	return 0;
}

static int
recvarpreply(NETDEV *dev, ARP *arp)
{
	// add to arp table
	addarptable(dev, arp->sha, ntohl(arp->sip));

	return 0;
}

int
recvarp(NETDEV *dev, SKBUF *buf)
{
	ARP *arp;
	size_t size = buf->size;

	if (size < sizeof(ARP)) {
		return -1;
	}

	arp = (ARP *)buf->data;

	if (ntohs(arp->pro) != ETHER_TYPE_IPV4) {
		return -1;
	}

	switch (ntohs(arp->op)) {
	case ARPOP_REQUEST:
		return recvarpreq(dev, arp);
	case ARPOP_REPLY:
		return recvarpreply(dev, arp);
	default:
		break;
	}

	return 0;
}
