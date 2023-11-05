#ifndef __ROUTER_ICMP_H
#define __ROUTER_ICMP_H

#include "types.h"

typedef struct ICMPHDR ICMPHDR;
typedef struct ICMPECHO ICMPECHO;
typedef struct ICMP_TIME_EXCEEDED ICMP_TIME_EXCEEDED;
typedef struct ICMP_UNREACHABLE ICMP_UNREACHABLE;

// ICMP message type
#define	ICMP_TYPE_ECHO_REQUEST			8
#define	ICMP_TYPE_ECHO_RESPONSE			0
#define	ICMP_TYPE_DESTINATION_UNREACHABLE	3
#define	ICMP_TYPE_TIME_EXCEEDED			11
#define	ICMP_TYPE_INFORMATION_REQUEST		15
#define	ICMP_TYPE_INFORMATION_REPLY		16

struct ICMPHDR {
	uchar type;
	uchar code;
	ushort checksum;
};

struct ICMPECHO {
	ushort ident;
	ushort seq;
	uchar data[0];
};

struct ICMP_TIME_EXCEEDED {
	uint unused;
	uchar data[0];
};

struct ICMP_UNREACHABLE {
	uint unused;
	uchar data[0];
};

#endif	// __ROUTER_ICMP_H
