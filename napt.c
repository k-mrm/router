#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/if_ether.h>

#include "types.h"
#include "net.h"
#include "icmp.h"
#include "napt.h"

// port??
void
confignat(IP inip, IP inmask, IP out)
{
	IP insub = inip & inmask;
	NETDEV *in;
	char dbg[16];

	in = devbysubnet(insub);

	if (!in) {
		bug("?");
		return;
	}

	printf("NAT: %s out %s\n", in->name, ipv4addrfmt(out, dbg));

	if (!in->napt) {
		in->napt = malloc(sizeof *in->napt);
		if (!in->napt) {
			return;
		}
		memset (in->napt, 0, sizeof *in->napt);
		in->napt->out = out;
	}
}

typedef enum PORTTYPE PORTTYPE;

enum PORTTYPE {
	DST,
	SRC,
};

static int
getport(SKBUF *buf, uchar proto, PORTTYPE type, ushort *port)
{
	void *packet = buf->data;

	switch (proto) {
	case IPPROTO_TCP: {
		TCP_HDR *tcp = packet;

		if (type == DST) {
			*port = ntohs(tcp->dstport);
		}
		else {	// SRC
			*port = ntohs(tcp->srcport);
		}

		return 0;
	}
	case IPPROTO_UDP: {
		UDP_HDR *udp = packet;

		if (type == DST) {
			*port = ntohs(udp->dstport);
		}
		else {	// SRC
			*port = ntohs(udp->srcport);
		}

		return 0;
	}
	case IPPROTO_ICMP: {
		ICMP_ECHO *icmp = packet;

		if (icmp->type != ICMP_TYPE_ECHO_REQUEST &&
		    icmp->type != ICMP_TYPE_ECHO_RESPONSE) {
			// Not supported
			return -1;
		}

		*port = ntohs(icmp->ident);

		return 0;
	}
	default:
		return -1;
	}
}

static NAPT_ENT *
getentries(NAPT_TABLE *ntab, uchar proto, size_t *pnentry, int *pportmin)
{
	NAPT_ENT *table;
	size_t nentry;
	int min;

	switch (proto) {
	case IPPROTO_UDP:
		table = ntab->udp;
		nentry = 40000;
		min = 20000;
		break;
	case IPPROTO_TCP:
		table = ntab->tcp;
		nentry = 40000;
		min = 20000;
		break;
	case IPPROTO_ICMP:
		table = ntab->icmp;
		nentry = 0x10000;
		min = 0;
		break;
	default:
		bug("unreachable");
		return NULL;
	}

	if (pnentry) {
		*pnentry = nentry;
	}
	if (pportmin) {
		*pportmin = min;
	}

	return table;
}

static NAPT_ENT *
naptlsearch(NAPT_TABLE *ntab, uchar proto, IP lip, ushort lport)
{
	NAPT_ENT *table;
	NAPT_ENT *ent;
	size_t nentry;

	table = getentries(ntab, proto, &nentry, NULL);
	if (!table) {
		return NULL;
	}

	for (size_t i = 0; i < nentry; i++) {
		ent = table + i;
		if (ent->lip == lip && ent->lport == lport) {
			return ent;
		}
	}

	return NULL;
}

static NAPT_ENT *
naptgsearch(NAPT_TABLE *ntab, uchar proto, IP gip, ushort gport)
{
	NAPT_ENT *entry;
	NAPT_ENT *table;
	int min;
	ushort offset;

	table = getentries(ntab, proto, NULL, &min);
	if (!table) {
		return NULL;
	}

	offset = gport - min;
	entry = table + offset;

	if (entry->gip == gip && entry->gport == gport) {
		return entry;
	}
	else {
		return NULL;
	}
}

static NAPT_ENT *
allocgport(NAPT_TABLE *ntab, uchar proto, ushort port)
{
	NAPT_ENT *entry;
	NAPT_ENT *table;
	size_t nentry;
	ushort off;
	int min;

	table = getentries(ntab, proto, &nentry, &min);
	if (!table) {
		return NULL;
	}

	if (port < min || min + nentry <= port) {
		goto reallocate;
	}

	off = port - min;
	entry = table + off;
	if (entry->gport == 0) {
		entry->gport = port;
		return entry;
	}

reallocate:
	for (size_t i = 0; i < nentry; i++) {
		entry = table + i;
		if (entry->gport == 0) {
			entry->gport = i + min;
			return entry;
		}
	}

	return NULL;
}

static int
rebuildip(SKBUF *buf, NAPT_ENT *ent, NAT_DIRECTION dir)
{
	struct iphdr *iphdr = buf->iphdr;

	if (dir == INCOMING) {
		// Global -> Local
		iphdr->daddr = htonl(ent->lip);
	}
	else {	// OUTGOING
		// Local -> Global
		iphdr->saddr = htonl(ent->gip);
	}

	iphdr->check = 0;
	iphdr->check = ipchecksum(buf);
	printf("REBUILD IP: CHECKSUM=%x\n", iphdr->check);

	return 0;
}

static int
rebuildtcp(SKBUF *buf, NAPT_ENT *ent, NAT_DIRECTION dir)
{
	TCP_HDR *tcp = buf->data;
	IP src, dst;
	IP_PSEUDO ph;

	if (dir == INCOMING) {
		// Global -> Local
		tcp->dstport = htons(ent->lport);
		dst = ent->lip;
		src = ntohl(buf->iphdr->saddr);
	}
	else {	// OUTGOING
		// Local -> Global
		dst = ntohl(buf->iphdr->daddr);
		src = ent->gip;
	}

	ph.srcip = htonl(src);
	ph.dstip = htonl(dst);
	ph.reserved = 0;
	ph.proto = IPPROTO_TCP;
	ph.len = htons(buf->size);

	tcp->checksum = 0;
	tcp->checksum = checksum2((uchar *)&ph, sizeof ph, (uchar *)tcp, buf->size);

	return rebuildip(buf, ent, dir);
}

static int
rebuildudp(SKBUF *buf, NAPT_ENT *ent, NAT_DIRECTION dir)
{
	UDP_HDR *udp = buf->data;
	IP src, dst;
	IP_PSEUDO ph;

	if (dir == INCOMING) {
		// Global -> Local
		udp->dstport = htons(ent->lport);
		dst = ent->lip;
		src = ntohl(buf->iphdr->saddr);
	}
	else {	// OUTGOING
		// Local -> Global
		udp->srcport = htons(ent->gport);
		dst = ntohl(buf->iphdr->daddr);
		src = ent->gip;
	}

	ph.srcip = htonl(src);
	ph.dstip = htonl(dst);
	ph.reserved = 0;
	ph.proto = IPPROTO_UDP;
	ph.len = htons(buf->size);

	udp->checksum = 0;
	udp->checksum = checksum2((uchar *)&ph, sizeof ph, (uchar *)udp, buf->size);
	if (dir == OUTGOING)
		printf("OOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOO UDP CHECKSUM=%x\n", udp->checksum);

	return rebuildip(buf, ent, dir);
}

static int
rebuildicmp(SKBUF *buf, NAPT_ENT *ent, NAT_DIRECTION dir)
{
	ICMP_ECHO *icmp = buf->data;

	if (dir == INCOMING) {
		// Global -> Local
		icmp->ident = htons(ent->lport);
	}
	else {	// OUTGOING
		// Local -> Global
		icmp->ident = htons(ent->gport);
	}
	
	icmp->checksum = 0;
	icmp->checksum = checksum((uchar *)icmp, buf->size);

	return rebuildip(buf, ent, dir);
}

static int
rebuild(SKBUF *buf, uchar proto, NAPT_ENT *ent, NAT_DIRECTION dir)
{
	switch (proto) {
	case IPPROTO_TCP:
		return rebuildtcp(buf, ent, dir);
	case IPPROTO_UDP:
		return rebuildudp(buf, ent, dir);
	case IPPROTO_ICMP:
		return rebuildicmp(buf, ent, dir);
	default:
		bug("unreachable");
		return -1;
	}
}

static int
naptin(NAPT *napt, SKBUF *buf)
{
	struct iphdr *iphdr;
	uchar proto;
	ushort gport;
	int rc;
	NAPT_ENT *ent;
	IP gip;

	iphdr = buf->iphdr;
	proto = iphdr->protocol;
	gip = ntohl(iphdr->daddr);

	rc = getport(buf, proto, DST, &gport);
	if (rc < 0) {
		return -1;
	}

	if (proto == IPPROTO_TCP) {
		printf("INM TCP!!!!!!!!  ");
	}
	printf("NAPT search : proto=%d gip=%x gport=%d ", proto, gip, gport);
	ent = naptgsearch(&napt->table, proto, gip, gport);
	if (!ent) {
		printf(" failed\n");
		return -1;
	}
	printf(" get lip=%x lport=%d\n", ent->lip, ent->lport);

	// rewrite packet
	return rebuild(buf, proto, ent, INCOMING);
}

static int
naptout(NAPT *napt, SKBUF *buf)
{
	struct iphdr *iphdr;
	uchar proto;
	NAPT_ENT *ent;
	IP lip, gip;
	ushort lport;
	int rc;

	iphdr = buf->iphdr;
	proto = iphdr->protocol;
	lip = ntohl(iphdr->saddr);
	gip = napt->out;

	rc = getport(buf, proto, SRC, &lport);
	if (rc < 0) {
		return -1;
	}

	ent = naptlsearch(&napt->table, proto, lip, lport);

	if (!ent) {
		ent = allocgport(&napt->table, proto, lport);
		if (!ent) {
			return -1;
		}

		ent->lip = lip;
		ent->gip = gip;
		ent->lport = lport;
	}

	if (proto == IPPROTO_TCP) {
		printf("OUT: TCP!!!!!!!!  ");
	}
	printf("NAPT local : lip=%x gip=%x lport=%d gport=%d\n", lip, gip, lport, ent->gport);

	// rebuild packet
	return rebuild(buf, proto, ent, OUTGOING);
}

int
napt(NAPT *napt, NAT_DIRECTION d, SKBUF *buf)
{
	if (!napt) {
		return -1;
	}

	if (d == INCOMING) {
		return naptin(napt, buf);
	}
	else if (d == OUTGOING) {
		return naptout(napt, buf);
	}
	else {
		bug("unreachable");
		return -1;
	}
}
