/*
 * network.h
 *
 * Header file for network.c source code
 */

/*
    NNGS - The No Name Go Server
    Copyright (C) 1995  Bill Shubert (wms@hevanet.com)

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.
*/


#ifndef NETWORK_H
#define NETWORK_H

/**********************************************************************
 * Constants
 **********************************************************************/

/*
 * Connections that come in on fd's larger than CONNECTION_COUNT will be
 *   immediately closed.
 */
#ifdef __CYGWIN__
#define  CONNECTION_COUNT  64
#else
#define  CONNECTION_COUNT  200
#endif
extern int net_fd_count;

/*
 * NET_THROTTLE is the maximum amount of outbound data that can be queued
 *   up before the system stops accepting more input from a connection.
 */
#define  NET_THROTTLE (4*1024)

/*
 * This is the default size for an output buffer.  Buffers may grow past
 *   this length on an as-needed basis, but it is computationally costly.
 *   All output buffers will be at least this length, so making this
 *   excessively large will cost memory.
 */
#define  NET_OUTBUFSZ (4*1024)

/*
 * The maximum number of ports that you can have listening for incoming
 *   connections.
 */
#define  LISTEN_COUNT  5


/**********************************************************************
 * Functions
 **********************************************************************/
/*
 * net_init must be called before any other network code.  It initializes
 *   the network data structures and starts listening on port "port" for
 *   incoming connections.
 * It returns 0 for success, -1 for error.
 */
int  net_init(int portnum, int want_udp);

/*
 */
void  net_select(int timeout);

/*
 * net_send sends the data in question out the file descriptor.  net_sendStr
 *   is a macro that sends a null-terminated ascii string.
 */
int  net_send(int, const char *, int);
#define  net_sendStr(fd, str)  \
  do  {  \
    const char *ttmp = (str);  \
    net_send(fd, ttmp, strlen(ttmp));  \
  } while(0)

/*
 * net_close  { flush fd; close it; cleanup netslot }
 */
void  net_close(int);
/*
 * net_closeAll will flush and close all fds.
 */
void  net_closeAll(void);

int  net_connectedHost(int);
void  net_echoOn(int);
void  net_echoOff(int);
/*
 * net_isalive() returns 1 if fd refers to an (active) client fd
 * , 0 otherwise
 */
int  net_isalive(int);

	/* format netslot into *static* buffer. For debugging/UDP */
char * net_dumpslot(int fd);
#endif	/* NETWORK_H */
