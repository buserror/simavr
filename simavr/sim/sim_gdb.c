/*
	sim_gdb.c

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
#include <sys/time.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <poll.h>
#include <pthread.h>
#include "sim_avr.h"
#include "sim_hex.h"
#include "avr_eeprom.h"
#include "sim_gdb.h"

#define DBG(w)

#define WATCH_LIMIT (32)

typedef struct {
	uint32_t len; /**< How many points are taken (points[0] .. points[len - 1]). */
	struct {
		uint32_t addr; /**< Which address is watched. */
		uint32_t size; /**< How large is the watched segment. */
		uint32_t kind; /**< Bitmask of enum avr_gdb_watch_type values. */
	} points[WATCH_LIMIT];
} avr_gdb_watchpoints_t;

typedef struct avr_gdb_t {
	avr_t * avr;
	int		listen;	// listen socket
	int		s;		// current gdb connection

	avr_gdb_watchpoints_t breakpoints;
	avr_gdb_watchpoints_t watchpoints;
} avr_gdb_t;


/**
 * Returns the index of the watchpoint if found, -1 otherwise.
 */
static int gdb_watch_find(const avr_gdb_watchpoints_t * w, uint32_t addr)
{
	for (int i = 0; i < w->len; i++) {
		if (w->points[i].addr > addr) {
			return -1;
		} else if (w->points[i].addr == addr) {
			return i;
		}
	}

	return -1;
}

/**
 * Contrary to gdb_watch_find, this actually checks the address against
 * a watched memory _range_.
 */
static int gdb_watch_find_range(const avr_gdb_watchpoints_t * w, uint32_t addr)
{
	for (int i = 0; i < w->len; i++) {
		if (w->points[i].addr > addr) {
			return -1;
		} else if (w->points[i].addr <= addr && addr < w->points[i].addr + w->points[i].size) {
			return i;
		}
	}

	return -1;
}

/**
 * Returns -1 on error, 0 otherwise.
 */
static int gdb_watch_add_or_update(avr_gdb_watchpoints_t * w, enum avr_gdb_watch_type kind, uint32_t addr,
		uint32_t size)
{
	/* If the watchpoint exists, update it. */
	int i = gdb_watch_find(w, addr);
	if (i != -1) {
		w->points[i].size = size;
		w->points[i].kind |= kind;
		return 0;
	}

	/* Otherwise add it. */
	if (w->len == WATCH_LIMIT) {
		return -1;
	}

	/* Find the insertion point. */
	for (i = 0; i < w->len; i++) {
		if (w->points[i].addr > addr) {
			break;
		}
	}

	w->len++;

	/* Make space for new element. */
	for (int j = i + 1; j < w->len; j++) {
		w->points[j] = w->points[j - 1];
	}

	/* Insert it. */
	w->points[i].kind = kind;
	w->points[i].addr = addr;
	w->points[i].size = size;

	return 0;
}

/**
 * Returns -1 on error or if the specified point does not exist, 0 otherwise.
 */
static int gdb_watch_rm(avr_gdb_watchpoints_t * w, enum avr_gdb_watch_type kind, uint32_t addr)
{
	int i = gdb_watch_find(w, addr);
	if (i == -1) {
		return -1;
	}

	w->points[i].kind &= ~kind;
	if (w->points[i].kind) {
		return 0;
	}

	for (i = i + 1; i < w->len; i++) {
		w->points[i - 1] = w->points[i];
	}

	w->len--;

	return 0;
}

static void gdb_watch_clear(avr_gdb_watchpoints_t * w)
{
	w->len = 0;
}

static void gdb_send_reply(avr_gdb_t * g, char * cmd)
{
	uint8_t reply[1024];
	uint8_t * dst = reply;
	uint8_t check = 0;
	*dst++ = '$';
	while (*cmd) {
		check += *cmd;
		*dst++ = *cmd++;
	}
	sprintf((char*)dst, "#%02x", check);
	DBG(printf("%s '%s'\n", __FUNCTION__, reply);)
	send(g->s, reply, dst - reply + 3, 0);
}

static void gdb_send_quick_status(avr_gdb_t * g, uint8_t signal)
{
	char cmd[64];

	sprintf(cmd, "T%02x20:%02x;21:%02x%02x;22:%02x%02x%02x00;",
		signal ? signal : 5, g->avr->data[R_SREG], 
		g->avr->data[R_SPL], g->avr->data[R_SPH],
		g->avr->pc & 0xff, (g->avr->pc>>8)&0xff, (g->avr->pc>>16)&0xff);
	gdb_send_reply(g, cmd);
}

static int gdb_change_breakpoint(avr_gdb_watchpoints_t * w, int set, enum avr_gdb_watch_type kind,
		uint32_t addr, uint32_t size)
{
	DBG(printf("set %d kind %d addr %08x len %d\n", set, kind, addr, len);)

	if (set) {
		return gdb_watch_add_or_update(w, kind, addr, size);
	} else {
		return gdb_watch_rm(w, kind, addr);
	}

	return -1;
}

static int gdb_write_register(avr_gdb_t * g, int regi, uint8_t * src)
{
	switch (regi) {
		case 0 ... 31:
			g->avr->data[regi] = *src;
			return 1;
		case 32:
			g->avr->data[R_SREG] = *src;
			return 1;
		case 33:
			g->avr->data[R_SPL] = src[0];
			g->avr->data[R_SPH] = src[1];
			return 2;
		case 34:
			g->avr->pc = src[0] | (src[1] << 8) | (src[2] << 16) | (src[3] << 24);
			return 4;
	}
	return 1;
}

static int gdb_read_register(avr_gdb_t * g, int regi, char * rep)
{
	switch (regi) {
		case 0 ... 31:
			sprintf(rep, "%02x", g->avr->data[regi]);
			break;
		case 32:
			sprintf(rep, "%02x", g->avr->data[R_SREG]);
			break;
		case 33:
			sprintf(rep, "%02x%02x", g->avr->data[R_SPL], g->avr->data[R_SPH]);
			break;
		case 34:
			sprintf(rep, "%02x%02x%02x00", 
				g->avr->pc & 0xff, (g->avr->pc>>8)&0xff, (g->avr->pc>>16)&0xff);
			break;
	}
	return strlen(rep);
}

static void gdb_handle_command(avr_gdb_t * g, char * cmd)
{
	avr_t * avr = g->avr;
	char rep[1024];
	uint8_t command = *cmd++;
	switch (command) {
		case '?':
			gdb_send_quick_status(g, 0);
			break;
		case 'G': {	// set all general purpose registers
			// get their binary form
			read_hex_string(cmd, (uint8_t*)rep, strlen(cmd));
			uint8_t *src = (uint8_t*)rep;
			for (int i = 0; i < 35; i++)
				src += gdb_write_register(g, i, src);
			gdb_send_reply(g, "OK");										
		}	break;
		case 'g': {	// read all general purpose registers
			char * dst = rep;
			for (int i = 0; i < 35; i++)
				dst += gdb_read_register(g, i, dst);
			gdb_send_reply(g, rep);						
		}	break;
		case 'p': {	// read register
			unsigned int regi = 0;
			sscanf(cmd, "%x", &regi);
			gdb_read_register(g, regi, rep);
			gdb_send_reply(g, rep);			
		}	break;
		case 'P': {	// write register
			unsigned int regi = 0;
			char * val = strchr(cmd, '=');
			if (!val)
				break;
			*val++ = 0;
			sscanf(cmd, "%x", &regi);
			read_hex_string(val, (uint8_t*)rep, strlen(val));
			gdb_write_register(g, regi, (uint8_t*)rep);
			gdb_send_reply(g, "OK");										
		}	break;
		case 'm': {	// read memory
			avr_flashaddr_t addr;
			uint32_t len;
			sscanf(cmd, "%x,%x", &addr, &len);
			uint8_t * src = NULL;
			if (addr < avr->flashend) {
				src = avr->flash + addr;
			} else if (addr >= 0x800000 && (addr - 0x800000) <= avr->ramend) {
				src = avr->data + addr - 0x800000;
			} else if (addr >= 0x810000 && (addr - 0x810000) <= avr->e2end) {
				avr_eeprom_desc_t ee = {.offset = (addr - 0x810000)};
				avr_ioctl(avr, AVR_IOCTL_EEPROM_GET, &ee);
				if (ee.ee)
					src = ee.ee;
				else {
					gdb_send_reply(g, "E01");
					break;
				}
			} else if (addr >= 0x800000 && (addr - 0x800000) == avr->ramend+1 && len == 2) {
				// Allow GDB to read a value just after end of stack.
				// This is necessary to make instruction stepping work when stack is empty
				printf("GDB read just past end of stack %08x, %08x; returning zero\n", addr, len);
				gdb_send_reply(g, "0000");
				break;
			} else {
				printf("read memory error %08x, %08x (ramend %04x)\n", addr, len, avr->ramend+1);
				gdb_send_reply(g, "E01");
				break;
			}
			char * dst = rep;
			while (len--) {
				sprintf(dst, "%02x", *src++);
				dst += 2;
			}
			*dst = 0;
			gdb_send_reply(g, rep);
		}	break;
		case 'M': {	// write memory
			uint32_t addr, len;
			sscanf(cmd, "%x,%x", &addr, &len);
			char * start = strchr(cmd, ':');
			if (!start) {
				gdb_send_reply(g, "E01");
				break;
			}
			if (addr < 0xffff) {
				read_hex_string(start + 1, avr->flash + addr, strlen(start+1));
				gdb_send_reply(g, "OK");			
			} else if (addr >= 0x800000 && (addr - 0x800000) <= avr->ramend) {
				read_hex_string(start + 1, avr->data + addr - 0x800000, strlen(start+1));
				gdb_send_reply(g, "OK");							
			} else if (addr >= 0x810000 && (addr - 0x810000) <= avr->e2end) {
				read_hex_string(start + 1, (uint8_t*)rep, strlen(start+1));
				avr_eeprom_desc_t ee = {.offset = (addr - 0x810000), .size = len, .ee = (uint8_t*)rep };
				avr_ioctl(avr, AVR_IOCTL_EEPROM_SET, &ee);
				gdb_send_reply(g, "OK");							
			} else {
				printf("write memory error %08x, %08x\n", addr, len);
				gdb_send_reply(g, "E01");
			}		
		}	break;
		case 'c': {	// continue
			avr->state = cpu_Running;
		}	break;
		case 's': {	// step
			avr->state = cpu_Step;
		}	break;
		case 'r': {	// deprecated, suggested for AVRStudio compatibility
			avr->state = cpu_StepDone;
			avr_reset(avr);
		}	break;
		case 'Z': 	// set clear break/watchpoint
		case 'z': {
			uint32_t kind, addr, len;
			int set = (command == 'Z');
			sscanf(cmd, "%d,%x,%x", &kind, &addr, &len);
//			printf("breakpoint %d, %08x, %08x\n", kind, addr, len);
			switch (kind) {
				case 0:	// software breakpoint
				case 1:	// hardware breakpoint
					if (addr > avr->flashend ||
							gdb_change_breakpoint(&g->breakpoints, set, 1 << kind, addr, len) == -1) {
						gdb_send_reply(g, "E01");
						break;
					}

					gdb_send_reply(g, "OK");
					break;
				case 2: // write watchpoint
				case 3: // read watchpoint
				case 4: // access watchpoint
					/* Mask out the offset applied to SRAM addresses. */
					addr &= ~0x800000;
					if (addr > avr->ramend ||
							gdb_change_breakpoint(&g->watchpoints, set, 1 << kind, addr, len) == -1) {
						gdb_send_reply(g, "E01");
						break;
					}

					gdb_send_reply(g, "OK");
					break;
				default:
					gdb_send_reply(g, "");
					break;
			}
		}	break;
		default:
			gdb_send_reply(g, "");
			break;
	}
}

static int gdb_network_handler(avr_gdb_t * g, uint32_t dosleep)
{
	fd_set read_set;
	int max;
	FD_ZERO(&read_set);

	if (g->s != -1) {
		FD_SET(g->s, &read_set);
		max = g->s + 1;
	} else {
		FD_SET(g->listen, &read_set);
		max = g->listen + 1;
	}
	struct timeval timo = { 0, dosleep };	// short, but not too short interval
	int ret = select(max, &read_set, NULL, NULL, &timo);

	if (ret == 0)
		return 0;
	
	if (FD_ISSET(g->listen, &read_set)) {
		g->s = accept(g->listen, NULL, NULL);

		if (g->s == -1) {
			perror("gdb_network_handler accept");
			sleep(5);
			return 1;
		}
        int i = 1;
        setsockopt (g->s, IPPROTO_TCP, TCP_NODELAY, &i, sizeof (i));
		g->avr->state = cpu_Stopped;
		printf("%s connection opened\n", __FUNCTION__);		
	}
		
	if (g->s != -1 && FD_ISSET(g->s, &read_set)) {
		uint8_t buffer[1024];
		
		ssize_t r = recv(g->s, buffer, sizeof(buffer)-1, 0);

		if (r == 0) {
			printf("%s connection closed\n", __FUNCTION__);
			close(g->s);
			gdb_watch_clear(&g->breakpoints);
			gdb_watch_clear(&g->watchpoints);
			g->avr->state = cpu_Running;	// resume
			g->s = -1;
			return 1;
		}
		if (r == -1) {
			perror("gdb_network_handler recv");
			sleep(1);
			return 1;
		}
		buffer[r] = 0;
	//	printf("%s: received %d bytes\n'%s'\n", __FUNCTION__, r, buffer);
	//	hdump("gdb", buffer, r);

		uint8_t * src = buffer;
		while (*src == '+' || *src == '-')
			src++;
		// control C -- lets send the guy a nice status packet
		if (*src == 3) {
			src++;
			g->avr->state = cpu_StepDone;
			printf("GDB hit control-c\n");
		}
		if (*src  == '$') {
			// strip checksum
			uint8_t * end = buffer + r - 1;
			while (end > src && *end != '#')
				*end-- = 0;
			*end = 0;
			src++;
			DBG(printf("GDB command = '%s'\n", src);)

			send(g->s, "+", 1, 0);

			gdb_handle_command(g, (char*)src);
		}
	}
	return 1;
}

/**
 * If an applicable watchpoint exists for addr, stop the cpu and send a status report.
 * type is one of AVR_GDB_WATCH_READ, AVR_GDB_WATCH_WRITE depending on the type of access.
 */
void avr_gdb_handle_watchpoints(avr_t * avr, uint16_t addr, enum avr_gdb_watch_type type)
{
	avr_gdb_t *g = avr->gdb;

	int i = gdb_watch_find_range(&g->watchpoints, addr);
	if (i == -1) {
		return;
	}

	int kind = g->watchpoints.points[i].kind;
	if (kind & type) {
		/* Send gdb reply (see GDB user manual appendix E.3). */
		char cmd[78];
		sprintf(cmd, "T%02x20:%02x;21:%02x%02x;22:%02x%02x%02x00;%s:%06x;",
				5, g->avr->data[R_SREG],
				g->avr->data[R_SPL], g->avr->data[R_SPH],
				g->avr->pc & 0xff, (g->avr->pc>>8)&0xff, (g->avr->pc>>16)&0xff,
				kind & AVR_GDB_WATCH_ACCESS ? "awatch" : kind & AVR_GDB_WATCH_WRITE ? "watch" : "rwatch",
				addr | 0x800000);
		gdb_send_reply(g, cmd);

		avr->state = cpu_Stopped;
	}
}

int avr_gdb_processor(avr_t * avr, int sleep)
{
	if (!avr || !avr->gdb)
		return 0;	
	avr_gdb_t * g = avr->gdb;

	if (avr->state == cpu_Running && gdb_watch_find(&g->breakpoints, avr->pc) != -1) {
		DBG(printf("avr_gdb_processor hit breakpoint at %08x\n", avr->pc);)
		gdb_send_quick_status(g, 0);
		avr->state = cpu_Stopped;
	} else if (avr->state == cpu_StepDone) {
		gdb_send_quick_status(g, 0);
		avr->state = cpu_Stopped;
	}
	// this also sleeps for a bit
	return gdb_network_handler(g, sleep);
}


int avr_gdb_init(avr_t * avr)
{
	avr_gdb_t * g = malloc(sizeof(avr_gdb_t));
	memset(g, 0, sizeof(avr_gdb_t));

	avr->gdb = NULL;

	if ((g->listen = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
		fprintf(stderr, "Can't create socket: %s", strerror(errno));
		return -1;
	}

	int i = 1;
	setsockopt(g->listen, SOL_SOCKET, SO_REUSEADDR, &i, sizeof(i));

	struct sockaddr_in address = { 0 };
	address.sin_family = AF_INET;
	address.sin_port = htons (avr->gdb_port);

	if (bind(g->listen, (struct sockaddr *) &address, sizeof(address))) {
		fprintf(stderr, "Can not bind socket: %s", strerror(errno));
		return -1;
	}
	if (listen(g->listen, 1)) {
		perror("listen");
		return -1;
	}
	printf("avr_gdb_init listening on port %d\n", avr->gdb_port);
	g->avr = avr;
	g->s = -1;
	avr->gdb = g;
	// change default run behaviour to use the slightly slower versions
	avr->run = avr_callback_run_gdb;
	avr->sleep = avr_callback_sleep_gdb;
	
	return 0;
}
