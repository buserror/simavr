/*
	sim_gdb.c

	Placeholder!

	Copyright 2008, 2009 Michel Pollet <buserror@gmail.com>

 	This file is part of simavr.

	simavr is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	simavr is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with simavr.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <poll.h>
#include "simavr.h"

typedef struct avr_gdb_t {
	avr_t * avr;
	int		sock;
} avr_gdb_t;

int avr_gdb_init(avr_t * avr)
{
	avr_gdb_t * g = malloc(sizeof(avr_gdb_t));
	memset(g, 0, sizeof(avr_gdb_t));

	avr->gdb = NULL;

	if ((g->sock = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
		fprintf(stderr, "Can't create socket: %s", strerror(errno));
		return -1;
	}

	int i = 1;
	setsockopt(g->sock, SOL_SOCKET, SO_REUSEADDR, &i, sizeof(i));

	struct sockaddr_in address = { 0 };
	address.sin_family = AF_INET;
	address.sin_port = htons (1234);

	if (bind(g->sock, (struct sockaddr *) &address, sizeof(address))) {
		fprintf(stderr, "Can not bind socket: %s", strerror(errno));
		return -1;
	}
	avr->gdb = g;
	return 0;
}
