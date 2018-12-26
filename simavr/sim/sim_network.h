/*
	sim_network.h

	Copyright 2012 Stephan Veigl <veigl@gmx.net>

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

#ifndef __SIM_NETWORK_H__
#define __SIM_NETWORK_H__

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __MINGW32__

// Windows with MinGW

#include <winsock2.h>
#include <windows.h>
#include <ws2tcpip.h>

#define send(sockfd, buf, len, flags) \
	(ssize_t)send( (sockfd), (const char *)(buf), (len), (flags))
#define setsockopt(sockfd, level, optname, optval, optlen) \
	setsockopt( (sockfd), (level), (optname), (void *)(optval), (optlen))
#define recv(sockfd, buf, len, flags) \
	(ssize_t)recv( (sockfd), (char *)(buf), (len), (flags))
#define sleep(x) Sleep((x)*1000)

static inline int network_init()
{
	// Windows requires WinSock to be init before use
	WSADATA wsaData;
	if ( WSAStartup( MAKEWORD( 2, 2 ), &wsaData ) )
		return -1;

	return 0;
}

static inline void network_release()
{
	// close WinSock
	WSACleanup();
}

#else

// native Linux

#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <poll.h>

static inline int network_init()
{
	// nothing to do
	return 0;
}

static inline void network_release()
{
	// nothing to do
}

#endif

#ifdef __cplusplus
};
#endif

#endif /*__SIM_NETWORK_H__*/
