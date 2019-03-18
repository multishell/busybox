/* vi: set sw=4 ts=4: */
/*
 * Utility routines.
 *
 * Connect to host at port using address resolusion from getaddrinfo
 *
 */

#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <errno.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "libbb.h"

/* Return network byte ordered port number for a service.
 * If "port" is a number use it as the port.
 * If "port" is a name it is looked up in /etc/services, if it isnt found return
 * default_port
 */
unsigned short bb_lookup_port(const char *port, unsigned short default_port)
{
	unsigned short port_nr = htons(default_port);
	if (port) {
	char *endptr;
		long port_long = strtol(port, &endptr, 10);

		if (errno != 0 || *endptr!='\0' || endptr==port || port_long < 0 || port_long > 65535) {
			struct servent *tserv = getservbyname(port, "tcp");
			if (tserv) {
			port_nr = tserv->s_port;
		}
	} else {
			port_nr = htons(port_long);
		}
	}
	return port_nr;
}

void bb_lookup_host(struct sockaddr_in *s_in, const char *host)
{
	struct hostent *he;

	memset(s_in, 0, sizeof(struct sockaddr_in));
	s_in->sin_family = AF_INET;
	he = xgethostbyname(host);
	memcpy(&(s_in->sin_addr), he->h_addr_list[0], he->h_length);
}

int xconnect(struct sockaddr_in *s_addr)
{
	int s = socket(AF_INET, SOCK_STREAM, 0);
	if (connect(s, (struct sockaddr_in *)s_addr, sizeof(struct sockaddr_in)) < 0)
	{
		bb_perror_msg_and_die("Unable to connect to remote host (%s)", 
				inet_ntoa(s_addr->sin_addr));
	}
	return s;
}
