/* vi: set sw=4 ts=4: */
/*
 * $Id: ping.c,v 1.12 2000/04/13 18:49:43 erik Exp $
 * Mini ping implementation for busybox
 *
 * Copyright (C) 1999 by Randolph Chung <tausq@debian.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 * This version of ping is adapted from the ping in netkit-base 0.10,
 * which is:
 *
 * Copyright (c) 1989 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Mike Muuss.
 * 
 * Original copyright notice is retained at the end of this file.
 */

#include "internal.h"
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/file.h>
#include <sys/time.h>
#include <sys/times.h>
#include <sys/signal.h>

#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#define DEFDATALEN      56
#define	MAXIPLEN	60
#define	MAXICMPLEN	76
#define	MAXPACKET	65468
#define	MAX_DUP_CHK	(8 * 128)
#define MAXWAIT         10
#define PINGINTERVAL    1		/* second */

#define O_QUIET         (1 << 0)

#define	A(bit)		rcvd_tbl[(bit)>>3]	/* identify byte in array */
#define	B(bit)		(1 << ((bit) & 0x07))	/* identify bit in byte */
#define	SET(bit)	(A(bit) |= B(bit))
#define	CLR(bit)	(A(bit) &= (~B(bit)))
#define	TST(bit)	(A(bit) & B(bit))

/* common routines */
static int in_cksum(unsigned short *buf, int sz)
{
	int nleft = sz;
	int sum = 0;
	unsigned short *w = buf;
	unsigned short ans = 0;

	while (nleft > 1) {
		sum += *w++;
		nleft -= 2;
	}

	if (nleft == 1) {
		*(unsigned char *) (&ans) = *(unsigned char *) w;
		sum += ans;
	}

	sum = (sum >> 16) + (sum & 0xFFFF);
	sum += (sum >> 16);
	ans = ~sum;
	return (ans);
}

/* simple version */
#ifdef BB_SIMPLE_PING
static const char *ping_usage = "ping host\n\n";

static char *hostname = NULL;

static void noresp(int ign)
{
	printf("No response from %s\n", hostname);
	exit(0);
}

static int ping(const char *host)
{
	struct hostent *h;
	struct sockaddr_in pingaddr;
	struct icmp *pkt;
	int pingsock, c;
	char packet[DEFDATALEN + MAXIPLEN + MAXICMPLEN];

	if ((pingsock = socket(AF_INET, SOCK_RAW, 1)) < 0) {	/* 1 == ICMP */
		perror("ping");
		exit(1);
	}

	/* drop root privs if running setuid */
	setuid(getuid());

	memset(&pingaddr, 0, sizeof(struct sockaddr_in));

	pingaddr.sin_family = AF_INET;
	if (!(h = gethostbyname(host))) {
		fprintf(stderr, "ping: unknown host %s\n", host);
		exit(1);
	}
	memcpy(&pingaddr.sin_addr, h->h_addr, sizeof(pingaddr.sin_addr));
	hostname = h->h_name;

	pkt = (struct icmp *) packet;
	memset(pkt, 0, sizeof(packet));
	pkt->icmp_type = ICMP_ECHO;
	pkt->icmp_cksum = in_cksum((unsigned short *) pkt, sizeof(packet));

	c = sendto(pingsock, packet, sizeof(packet), 0,
			   (struct sockaddr *) &pingaddr, sizeof(struct sockaddr_in));

	if (c < 0 || c != sizeof(packet)) {
		if (c < 0)
			perror("ping");
		fprintf(stderr, "ping: write incomplete\n");
		exit(1);
	}

	signal(SIGALRM, noresp);
	alarm(5);					/* give the host 5000ms to respond */
	/* listen for replies */
	while (1) {
		struct sockaddr_in from;
		size_t fromlen = sizeof(from);

		if ((c = recvfrom(pingsock, packet, sizeof(packet), 0,
						  (struct sockaddr *) &from, &fromlen)) < 0) {
			if (errno == EINTR)
				continue;
			perror("ping");
			continue;
		}
		if (c >= 76) {			/* ip + icmp */
			struct iphdr *iphdr = (struct iphdr *) packet;

			pkt = (struct icmp *) (packet + (iphdr->ihl << 2));	/* skip ip hdr */
			if (pkt->icmp_type == ICMP_ECHOREPLY)
				break;
		}
	}
	printf("%s is alive!\n", hostname);
	return (TRUE);
}

extern int ping_main(int argc, char **argv)
{
	argc--;
	argv++;
	if (argc < 1)
		usage(ping_usage);
	ping(*argv);
	exit(TRUE);
}

#else
/* full(er) version */
static const char *ping_usage = "ping [OPTION]... host\n\n"
	"Send ICMP ECHO_REQUEST packets to network hosts.\n\n"
	"Options:\n"
	"\t-c COUNT\tSend only COUNT pings.\n"
	"\t-q\t\tQuiet mode, only displays output at start\n"
	"\t\t\tand when finished.\n"; 

static char *hostname = NULL;
static struct sockaddr_in pingaddr;
static int pingsock = -1;

static long ntransmitted = 0, nreceived = 0, nrepeats = 0, pingcount = 0;
static int myid = 0, options = 0;
static unsigned long tmin = ULONG_MAX, tmax = 0, tsum = 0;
static char rcvd_tbl[MAX_DUP_CHK / 8];

static void sendping(int);
static void pingstats(int);
static void unpack(char *, int, struct sockaddr_in *);

static void ping(char *);

/**************************************************************************/

static void pingstats(int ign)
{
	signal(SIGINT, SIG_IGN);

	printf("\n--- %s ping statistics ---\n", hostname);
	printf("%ld packets transmitted, ", ntransmitted);
	printf("%ld packets received, ", nreceived);
	if (nrepeats)
		printf("%ld duplicates, ", nrepeats);
	if (ntransmitted)
		printf("%ld%% packet loss\n",
			   (ntransmitted - nreceived) * 100 / ntransmitted);
	if (nreceived)
		printf("round-trip min/avg/max = %lu.%lu/%lu.%lu/%lu.%lu ms\n",
			   tmin / 10, tmin % 10,
			   (tsum / (nreceived + nrepeats)) / 10,
			   (tsum / (nreceived + nrepeats)) % 10, tmax / 10, tmax % 10);
	exit(0);
}

static void sendping(int ign)
{
	struct icmp *pkt;
	int i;
	char packet[DEFDATALEN + 8];

	pkt = (struct icmp *) packet;

	pkt->icmp_type = ICMP_ECHO;
	pkt->icmp_code = 0;
	pkt->icmp_cksum = 0;
	pkt->icmp_seq = ntransmitted++;
	pkt->icmp_id = myid;
	CLR(pkt->icmp_seq % MAX_DUP_CHK);

	gettimeofday((struct timeval *) &packet[8], NULL);
	pkt->icmp_cksum = in_cksum((unsigned short *) pkt, sizeof(packet));

	i = sendto(pingsock, packet, sizeof(packet), 0,
			   (struct sockaddr *) &pingaddr, sizeof(struct sockaddr_in));

	if (i < 0 || i != sizeof(packet)) {
		if (i < 0)
			perror("ping");
		fprintf(stderr, "ping wrote %d chars; %d expected\n", i,
				sizeof(packet));
		exit(1);
	}

	signal(SIGALRM, sendping);
	if (pingcount == 0 || ntransmitted < pingcount) {	/* schedule next in 1s */
		alarm(PINGINTERVAL);
	} else {					/* done, wait for the last ping to come back */
		/* todo, don't necessarily need to wait so long... */
		signal(SIGALRM, pingstats);
		alarm(MAXWAIT);
	}
}

static void unpack(char *buf, int sz, struct sockaddr_in *from)
{
	struct icmp *icmppkt;
	struct iphdr *iphdr;
	struct timeval tv, *tp;
	int hlen, dupflag;
	unsigned long triptime;

	gettimeofday(&tv, NULL);

	/* check IP header */
	iphdr = (struct iphdr *) buf;
	hlen = iphdr->ihl << 2;
	/* discard if too short */
	if (sz < (DEFDATALEN + ICMP_MINLEN))
		return;

	sz -= hlen;
	icmppkt = (struct icmp *) (buf + hlen);

	if (icmppkt->icmp_type == ICMP_ECHOREPLY) {
		if (icmppkt->icmp_id != myid)
			return;				/* not our ping */
		++nreceived;
		tp = (struct timeval *) icmppkt->icmp_data;

		if ((tv.tv_usec -= tp->tv_usec) < 0) {
			--tv.tv_sec;
			tv.tv_usec += 1000000;
		}
		tv.tv_sec -= tp->tv_sec;

		triptime = tv.tv_sec * 10000 + (tv.tv_usec / 100);
		tsum += triptime;
		if (triptime < tmin)
			tmin = triptime;
		if (triptime > tmax)
			tmax = triptime;

		if (TST(icmppkt->icmp_seq % MAX_DUP_CHK)) {
			++nrepeats;
			--nreceived;
			dupflag = 1;
		} else {
			SET(icmppkt->icmp_seq % MAX_DUP_CHK);
			dupflag = 0;
		}

		if (options & O_QUIET)
			return;

		printf("%d bytes from %s: icmp_seq=%u", sz,
			   inet_ntoa(*(struct in_addr *) &from->sin_addr.s_addr),
			   icmppkt->icmp_seq);
		printf(" ttl=%d", iphdr->ttl);
		printf(" time=%lu.%lu ms", triptime / 10, triptime % 10);
		if (dupflag)
			printf(" (DUP!)");
		printf("\n");
	} else {
		fprintf(stderr,
				"Warning: unknown ICMP packet received (not echo-reply)\n");
	}
}

static void ping(char *host)
{
	struct protoent *proto;
	struct hostent *h;
	char buf[MAXHOSTNAMELEN];
	char packet[DEFDATALEN + MAXIPLEN + MAXICMPLEN];
	int sockopt;

	proto = getprotobyname("icmp");
	/* if getprotobyname failed, just silently force 
	 * proto->p_proto to have the correct value for "icmp" */
	if ((pingsock = socket(AF_INET, SOCK_RAW,
						   (proto ? proto->p_proto : 1))) < 0) {	/* 1 == ICMP */
		if (errno == EPERM) {
			fprintf(stderr, "ping: permission denied. (are you root?)\n");
		} else {
			perror("ping");
		}
		exit(1);
	}

	/* drop root privs if running setuid */
	setuid(getuid());

	memset(&pingaddr, 0, sizeof(struct sockaddr_in));

	pingaddr.sin_family = AF_INET;
	if (!(h = gethostbyname(host))) {
		fprintf(stderr, "ping: unknown host %s\n", host);
		exit(1);
	}

	if (h->h_addrtype != AF_INET) {
		fprintf(stderr,
				"ping: unknown address type; only AF_INET is currently supported.\n");
		exit(1);
	}

	pingaddr.sin_family = AF_INET;	/* h->h_addrtype */
	memcpy(&pingaddr.sin_addr, h->h_addr, sizeof(pingaddr.sin_addr));
	strncpy(buf, h->h_name, sizeof(buf) - 1);
	hostname = buf;

	/* enable broadcast pings */
	sockopt = 1;
	setsockopt(pingsock, SOL_SOCKET, SO_BROADCAST, (char *) &sockopt,
			   sizeof(sockopt));

	/* set recv buf for broadcast pings */
	sockopt = 48 * 1024;
	setsockopt(pingsock, SOL_SOCKET, SO_RCVBUF, (char *) &sockopt,
			   sizeof(sockopt));

	printf("PING %s (%s): %d data bytes\n",
		   hostname,
		   inet_ntoa(*(struct in_addr *) &pingaddr.sin_addr.s_addr),
		   DEFDATALEN);

	signal(SIGINT, pingstats);

	/* start the ping's going ... */
	sendping(0);

	/* listen for replies */
	while (1) {
		struct sockaddr_in from;
		size_t fromlen = sizeof(from);
		int c;

		if ((c = recvfrom(pingsock, packet, sizeof(packet), 0,
						  (struct sockaddr *) &from, &fromlen)) < 0) {
			if (errno == EINTR)
				continue;
			perror("ping");
			continue;
		}
		unpack(packet, c, &from);
		if (pingcount > 0 && nreceived >= pingcount)
			break;
	}
	pingstats(0);
}

extern int ping_main(int argc, char **argv)
{
	char *thisarg;

	argc--;
	argv++;
	options = 0;
	/* Parse any options */
	while (argc >= 1 && **argv == '-') {
		thisarg = *argv;
		thisarg++;
		switch (*thisarg) {
		case 'q':
			options |= O_QUIET;
			break;
		case 'c':
			argc--;
			argv++;
			pingcount = atoi(*argv);
			break;
		default:
			usage(ping_usage);
		}
		argc--;
		argv++;
	}
	if (argc < 1)
		usage(ping_usage);

	myid = getpid() & 0xFFFF;
	ping(*argv);
	exit(TRUE);
}
#endif

/*
 * Copyright (c) 1989 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Mike Muuss.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */