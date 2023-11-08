#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
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
#include "ether.h"

void
bug(const char *fmt)
{
	fprintf(stderr, "%s\n", fmt);
	term = true;
}

static void
sighandler(int sig)
{
	fprintf(stderr, "terminated: reason %d\n", sig);
	term = true;
}

void
sigtrap()
{
	signal(SIGINT, sighandler);
	signal(SIGTERM, sighandler);
}

char *
hwaddrfmt(uchar *mac, char *str)
{
	snprintf(str, 32, "%02x:%02x:%02x:%02x:%02x:%02x",
		 mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

	return str;
}

// ipaddr must be little-endian
char *
ipv4addrfmt(IP ipaddr, char *str)
{
	uchar *ip;

	ipaddr = htonl(ipaddr);
	ip = (uchar *)&ipaddr;

	snprintf(str, 16, "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);

	return str;
}

void
pnetdev(NETDEV *dev)
{
	char ip[16];
	char mask[16];
	char sub[16];
	char mac[32];

	hwaddrfmt(dev->hwaddr, mac);
	ipv4addrfmt(dev->ipv4.addr, ip);
	ipv4addrfmt(dev->ipv4.netmask, mask);
	ipv4addrfmt(dev->ipv4.subnet, sub);

	printf("%s: hw=%s ip=%s mask=%s sub=%s\n", dev->name, mac, ip, mask, sub);
}

NETDEV *
devbysubnet(IP subnet)
{
	NETDEV *dev;

	for (int i = 0; i < ndev; i++) {
		dev = netdev[i];
		if (dev->ipv4.subnet == subnet) {
			return dev;
		}
	}

	return NULL;
}

NETDEV *
devbyipaddr(IP addr)
{
	NETDEV *dev;

	for (int i = 0; i < ndev; i++) {
		dev = netdev[i];
		if (dev->ipv4.addr == addr) {
			return dev;
		}
	}

	return NULL;
}

NETDEV *
opennetdev(const char *name, bool promisc)
{
	struct ifreq ifr = {0};
	struct sockaddr_ll sa = {0};
	struct sockaddr_in *sin;
	NETDEV *dev;

	dev = malloc(sizeof *dev);
	if (!dev)
		return NULL;

	dev->name = name;

	dev->soc = socket(PF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
	if (dev->soc < 0) {
		perror("soc");
		goto err;
	}

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

	if (ioctl(dev->soc, SIOCGIFFLAGS, &ifr) < 0) {
		perror("SIOCGIFFLAGS");
		goto err;
	}

	if (!(ifr.ifr_flags & IFF_UP)) {
		printf("%s is down\n", name);
		goto err;
	}

	if (promisc) {
		ifr.ifr_flags |= IFF_PROMISC;

		if(ioctl(dev->soc, SIOCSIFFLAGS, &ifr) < 0) {
			perror("SIOCSIFFLAGS");
			goto err;
		}
	}

	// get MAC address
	if (ioctl(dev->soc, SIOCGIFHWADDR, &ifr) < 0) {
		perror("SIOCGIFHWADDR");
		goto err;
	}
	ethaddrcpy(dev->hwaddr, (uchar *)ifr.ifr_hwaddr.sa_data);

	// get IP address
	if (ioctl(dev->soc, SIOCGIFADDR, &ifr) < 0) {
		perror("SIOCFIFADDR");
		goto err;
	}

	if (ifr.ifr_addr.sa_family != PF_INET) {
		perror("no IPv4");
		goto err;
	}

	sin = (struct sockaddr_in *)&ifr.ifr_addr;
	dev->ipv4.addr = ntohl(sin->sin_addr.s_addr);

	// get netmask
	if (ioctl(dev->soc, SIOCGIFNETMASK, &ifr) < 0) {
		perror("SIOCGIFNETMASK");
		goto err;
	}

	sin = (struct sockaddr_in *)&ifr.ifr_netmask;
	dev->ipv4.netmask = ntohl(sin->sin_addr.s_addr);

	dev->ipv4.subnet = dev->ipv4.addr & dev->ipv4.netmask;

	return dev;

err:
	if (dev->soc >= 0)
		close(dev->soc);
	free(dev);

	return NULL;
}

ssize_t
sendpacket(NETDEV *dev, SKBUF *buf)
{
	ssize_t len = write(dev->soc, buf->data, buf->size);

	skfree(buf);

	return len;
}

ssize_t
recvpacket(NETDEV *dev, uchar *buf, size_t nbuf)
{
	return read(dev->soc, buf, nbuf);
}

SKBUF *
skalloc(size_t size)
{
	SKBUF *skb;
	void *buf;

	skb = malloc(sizeof *skb);
	if (!skb) {
		return NULL;
	}

	buf = malloc(size + 64/* header room */);
	if (!buf) {
		return NULL;
	}

	memset(skb, 0, sizeof *skb);
	memset(buf, 0, size + 64);

	skb->head = buf;
	skb->data = buf + 64;
	skb->tail = buf + size + 64;
	skb->size = size;
	skb->ref = 1;

	return skb;
}

void *
skpush(SKBUF *buf, size_t size)
{
	void *data;

	data = buf->data - size;
	if (data < buf->head) {
		return NULL;
	}

	buf->data = data;
	buf->size += size;

	return data;
}

void
skref(SKBUF *buf)
{
	buf->ref++;
}

void *
skpull(SKBUF *buf, size_t size)
{
	void *data, *old;

	old = buf->data;

	data = buf->data + size;
	if (data >= buf->tail) {
		return NULL;
	}

	buf->data = data;
	buf->size -= size;

	return old;
}

void *
skpulleth(SKBUF *buf)
{
	buf->eth = skpull(buf, sizeof(struct ether_header));

	return buf->eth;
}

void *
skpullip(SKBUF *buf)
{
	struct iphdr *ip;
	ushort iphdrsz;
	uchar proto;

	ip = buf->data;

	if (ip->version != 4) {
		// not ipv4 packet
		return NULL;
	}

	iphdrsz = ip->ihl << 2;
	proto = ip->protocol;

	buf->iphdr = skpull(buf, iphdrsz);
	buf->iphdrsz = iphdrsz;
	buf->ipproto = proto;

	return buf->iphdr;
}

ushort
ipchecksum(SKBUF *buf)
{
	uchar *hdr = (uchar *)buf->iphdr;
	ushort hdrsz = buf->iphdrsz;

	return checksum(hdr, hdrsz);
}

void
skcopy(SKBUF *buf, uchar *src, size_t n)
{
	// TODO: boundary check
	memcpy(buf->data, src, n);
}

void
skfree(SKBUF *skb)
{
	void *buf;

	if (!skb) {
		return;
	}

	if (!skb->ref) {
		bug("double free");
		return;
	}

	if (--skb->ref == 0) {
		buf = skb->head;

		free(buf);
		free(skb);
	}
}

ushort
checksum2(uchar *buf1, size_t n1, uchar *buf2, size_t n2)
{
	uint sum = 0;
	ushort *ptr = (ushort *)buf1;
	size_t i;

	for (i = n1; i > 1; i -= 2) {
		sum += *ptr;
		if (sum & 0x80000000) {
			sum = (sum & 0xffff) + (sum >> 16);
		}
		ptr++;
	}
	if (i == 1) {
		ushort a = 0;
		*(uchar *)&a = *(uchar *)ptr;
		sum += a;
	}

	while (sum >> 16) {
		sum = (sum & 0xffff) + (sum >> 16);
	}

	ptr = (ushort *)buf2;

	for (i = n2; i > 1; i -= 2) {
		sum += *ptr;
		if (sum & 0x80000000) {
			sum = (sum & 0xffff) + (sum >> 16);
		}
		ptr++;
	}
	if (i == 1) {
		ushort a = 0;
		*(uchar *)&a = *(uchar *)ptr;
		sum += a;
	}

	while (sum >> 16) {
		sum = (sum & 0xffff) + (sum >> 16);
	}
	
	return ~sum;
}

ushort
checksum(uchar *buf, size_t n)
{
	uint sum = 0;
	ushort *ptr = (ushort *)buf;
	size_t i;

	for (i = n; i > 1; i -= 2) {
		sum += *ptr;
		if (sum & 0x80000000) {
			sum = (sum & 0xffff) + (sum >> 16);
		}

		ptr++;
	}
	if (i == 1) {
		ushort a = 0;
		*(uchar *)&a = *(uchar *)ptr;
		sum += a;
	}

	while (sum >> 16) {
		sum = (sum & 0xffff) + (sum >> 16);
	}
	
	return ~sum;
}
