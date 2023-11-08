#ifndef __ROUTER_NAPT_H
#define __ROUTER_NAPT_H

#include "types.h"
#include "net.h"

typedef struct NAPT 		NAPT;
typedef struct NAPT_TABLE 	NAPT_TABLE;
typedef struct NAPT_ENT 	NAPT_ENT;
typedef struct TCP_HDR		TCP_HDR;
typedef struct UDP_HDR		UDP_HDR;
typedef struct ICMP_ECHO	ICMP_ECHO;
typedef enum NAT_DIRECTION	NAT_DIRECTION;

enum NAT_DIRECTION {
	INCOMING,
	OUTGOING,
};

// Local -> Global
struct NAPT_ENT {
	IP lip;
	IP gip;
	ushort lport;
	ushort gport;
};

struct NAPT_TABLE {
	NAPT_ENT tcp[40000];	// 20000-59999
	NAPT_ENT udp[40000];	// 20000-59999
	NAPT_ENT icmp[0x10000];	// icmp id
};

struct NAPT {
	NAPT_TABLE table;
	IP out;
};

struct TCP_HDR {
	ushort srcport;
	ushort dstport;
	uint seq;
	uint ack;
	uchar offset;
	uchar flag;
	ushort windowsize;
	ushort checksum;
	ushort urgptr;
};

struct UDP_HDR {
	ushort srcport;
	ushort dstport;
	ushort packetlen;
	ushort checksum;
};

struct ICMP_ECHO {
	uchar type;
	uchar code;
	ushort checksum;
	ushort ident;
	ushort seq;
	uchar data[0];
};

int napt(NAPT *napt, NAT_DIRECTION d, SKBUF *buf);
void confignat(IP inip, IP inmask, IP out);

#endif
