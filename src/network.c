/*
 * network.c
*/

/*
    NNGS - The No Name Go Server
    Copyright (C) 1995-1996  Bill Shubert (wms@hevanet.com)

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.
*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <ctype.h>

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif

#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif

#ifdef HAVE_NETDB_H
#include <netdb.h>
#endif


#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif

#ifdef HAVE_TIME_H
#include <time.h>
#endif

#ifdef TIME_WITH_SYS_TIME
#include <sys/time.h>
#endif

#ifdef ARPA_INET_H
#include <arpa/inet.h>
#endif

#ifdef HAVE_ARPA_TELNET_H
#include <arpa/telnet.h>
#endif

#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif           

#ifdef AIX
#include <sys/select.h>
#endif

#ifdef USING_DMALLOC
#include <dmalloc.h>
#define DMALLOC_FUNC_CHECK 1
#endif

#include "network.h"
#include "common.h"
#include "conffile.h"
#include "command.h"
#include "utils.h"

#define UNUSED(_p) (void)(_p)
/* [PEM]: We have to try this... */
#define WRITE_AT_ONCE 0

/*#define  buglog(x)  Logit x */
#define  buglog(x) 

/* [PEM]: Debugging. An attempt to find where things hangs sometimes. */
#if 0
#define TIMED  struct timeval t0, t1
#define TIME0  gettimeofday(&t0, NULL)
#define TIME1(S, L) \
  do { double dt0, dt1; \
    gettimeofday(&t1, NULL); \
    dt0 = t0.tv_sec + (t0.tv_usec / 1000000.0); \
    dt1 = t1.tv_sec + (t1.tv_usec / 1000000.0); \
    if (dt1 - dt0 >= (L)) \
      Logit("TIME: %s in %gs", S, dt1-dt0); \
  } while(0)
#else
#define TIMED
#define TIME0
#define TIME1(S, L)
#endif

/**********************************************************************
 * Data types
 **********************************************************************/

#define NETSTATE_EMPTY 0
#define NETSTATE_LISTENING 1
#define NETSTATE_CONNECTED 2
	/* fd has been closed, but there may still be data in the struct */
#define NETSTATE_DISCONNECTED 3
#define NETSTATE_UDP 4

struct netstruct {
  int netstate;
  int telnetState;
  unsigned int fromHost; /* IP-adress in host byte order */
  unsigned int fromport; /* UDP-port in host byte order */
  unsigned is_full :1;	/* read() queue; this seems to never be set! (since 1.1.12) */
  unsigned is_throttled :1;	/* set when partial write()s start to occur */
  
  	/* Output buffering reallocates its buffer when needed */
  unsigned  out_size;  /* allocated size */
  unsigned  out_used;  /* How many bytes waiting to be written? */
  char *out_buff; /* our send buffer, or NULL if none yet */

  	/* Input buffering uses a fixed size buffer */
  unsigned in_used; /* The amount of data in the in_buff. */
  unsigned in_end;  /* The end of the first command in the buffer. */
  unsigned parse_dst, parse_src;
  char  in_buff[MAX_STRING_LENGTH];
};


/**********************************************************************
 * Static variables
 **********************************************************************/

/*
 * netarray hold all data associated with an i/o connection.  It is indexed by
 *   file descriptor.
 */
static struct netstruct netarray[CONNECTION_COUNT];

/*
 * listen_count is the number of file descriptors we are listening on
 * net_fd_top is the highest filedscriptor in use; for select(net_fd_top+1,...);
 */
static int listen_count = 0;
static int net_fd_top = -1;

/*
 * We keep these fd_sets up to date so that we don't have to reconstruct them
 *   every time we want to do a select().
 */
static fd_set readSet, writeSet;

/*
 * Telnet stuff
 */
static unsigned char wont_echo[3] = {IAC, WONT, TELOPT_ECHO};
static unsigned char will_echo[3] = {IAC, WILL, TELOPT_ECHO};
static unsigned char will_tm[3] = {IAC, WILL, TELOPT_TM};
static unsigned char will_sga[3] = {IAC, WILL, TELOPT_SGA};
static unsigned char ayt[] = "[Responding to AYT: Yes, I'm here.]\n";
/**********************************************************************
 * Forward declarations
 **********************************************************************/

static int  do_accept(int fd);
static int  do_write(int fd);
static int  do_read(int fd);
static int  clearCmd(int fd);
static int  checkForCmd(int fd);
static void  fd_init(int fd);
static void  fd_cleanup(int fd);
static void  flushWrites(void);


#include "udp_commands.h"

static int  do_read_udp(int fd);
static char udpbuff[2*4096];
/**********************************************************************
 * Public data
 **********************************************************************/

int net_fd_count = 0;
/**********************************************************************
 * Functions
 **********************************************************************/

static void set_nonblocking(int fd)
{
  int flags;

  if ((flags = fcntl(fd, F_GETFL, 0)) >= 0) {
    flags |= O_NONBLOCK;
    fcntl(fd, F_SETFL, flags);
  }
}

/*
 * Every time you call this, another fd is added to the listen list.
 */
int net_init(int portnum, int want_udp)
{
  static int doneinit = 0;
  int  fd,i;
  int  opt;
  struct sockaddr_in  addr;
  struct linger  lingerOpt;

  /* assert(COUNTOF(netarray) <= FD_SETSIZE); bogus */
  if (!doneinit)  {
    doneinit = 1;
    (void) refetch_ticker();
    for (i = 0;  i < COUNTOF(netarray);  ++i)  {
      /*
       * Set up all conns to be ignored.
       */
      netarray[i].netstate = NETSTATE_EMPTY;
      netarray[i].in_end = 0;
      netarray[i].out_used = 0;
      netarray[i].out_buff = NULL;
      netarray[i].in_used = 0;
      netarray[i].in_end = 0;
      netarray[i].parse_dst = 0;
      netarray[i].parse_src = 0;
      netarray[i].is_full = 0;
      netarray[i].is_throttled = 0;
    }
    net_fd_count = i;
    net_fd_top = 0;
    FD_ZERO(&readSet);
    FD_ZERO(&writeSet);
  }

  assert(listen_count < LISTEN_COUNT); /* Bogus */
  /* Open a TCP socket (an Internet stream socket). */
  if ((fd=socket(AF_INET, SOCK_STREAM, 0)) == -1) {
    fprintf(stderr, "NNGS: can't create stream socket\n");
    return -1;
  }
  /* Bind our local address so that the client can send to us */
  memset((void *)&addr, 0, sizeof addr);
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_ANY);
  addr.sin_port = htons(portnum);

  /* added in an attempt to allow rebinding to the port */
  opt = 1;
  setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof opt);
  opt = 1;
  setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &opt, sizeof opt);
  lingerOpt.l_onoff = 0;
  lingerOpt.l_linger = 0;
  setsockopt(fd, SOL_SOCKET, SO_LINGER, &lingerOpt, sizeof lingerOpt);

  if (conffile.debug_general > 99) {
    opt = 1;
    setsockopt(fd, SOL_SOCKET, SO_DEBUG, &opt, sizeof opt);
  }

  if (bind(fd, (struct sockaddr *)&addr, sizeof addr) == -1)  {
    fprintf(stderr, "NNGS: can't bind local address. errno=%d(%s)\n"
    , errno, strerror(errno));
    return -1;
  }
  set_nonblocking(fd);

  listen(fd, 5);

  netarray[fd].netstate = NETSTATE_LISTENING;
  if (fd > net_fd_top) net_fd_top = fd;
  FD_SET(fd , &readSet);
  listen_count++;

  if (want_udp>0) {
    int val,rc;

    if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
    fprintf(stderr, "NNGS: can't create UDP socket.  errno=%d\n", errno);
        return 0; /* we want to ignore this error */
        }

    val = 1;
    rc = setsockopt(fd, IPPROTO_IP , IP_RECVOPTS , &val, sizeof val);
    if (rc < 0) { rc = errno;
      fprintf(stderr, "Tinker := %d(%s)\n" , rc, strerror(rc) );
      close (fd);
      return -1;
    }

	/* Sorry, this is to minimize distribution errors.
	** I test on an old alpha, and want to listen on 0.0.0.0
	** Other people with alphas will probably want to block
	** port 9696/udp.
	** But (other) people with alphas have firewalls and know
	** what they are doing, off course ...
	*/
#ifndef __alpha
    addr.sin_addr.s_addr = htonl(0x7f000001); /* 127.0.0.1 */
#endif
    if (bind(fd, (struct sockaddr *) &addr, sizeof addr) == -1) {
      fprintf(stderr
      , "bind() Fd %d, Family %d, Port %d, Addr %x fails:%s\n"
      , fd
      , addr.sin_family
      , (int) ntohs(addr.sin_port)
      , (int) ntohl(addr.sin_addr.s_addr)
      , strerror(errno)
      );
      close(fd);
      return -1;
      }
    netarray[fd].netstate = NETSTATE_UDP;
    if (fd > net_fd_top) net_fd_top = fd;
    FD_SET(fd , &readSet);
    listen_count++;
  }

  return 0;
}


/*
 * net_select spins, doing work as it arrives.
 *
 * It is written to prevent any one user from sucking up too much server time
 *   by sending tons of commands at once. It consists essentially of two
 *   loops:
 *
 * for (;;) {
 *   select() to find out which connections have input data.
 *   Loop over all connections {
 *     If the connection has input data waiting, read it in.
 *     If the connection has data in its input buffer (from a previous loop
 *       or from just being read in), then process *ONE* command.
 *   }
 * }
 *
 * From this basic system there are some optimizations. If we manage to
 *   process every command waiting in an inner loop, then we sleep in the
 *   "select()" call since we have nothing else to do. If we leave a command
 *   in any of the input buffers (because there were two or more there
 *   before), then we don't wait in the "select()" and just poll.
 * Also, each connection has a maximum amount of data that will be buffered.
 *   Once the connection has all of its buffer space filled, we no longer
 *   try to read in more data. This will make the data back up on the client,
 *   keeping us from using up too much memory when a client sends tons of
 *   commands at once.
 */
void  net_select(int timeout)
{
  int  fd, selected;
  fd_set  tmp_read, tmp_write;
  struct timeval  timer;
  /*
   * When moreWork is TRUE, that means that we have data we have read in but
   *   not processed yet, so we should poll with the select() instead of
   *   sleep for a second.
   */
  int  moreWork = 0;
  int  elapsed, newConn;
  TIMED;

  UNUSED(timeout);
  /*
   * If we returned a command from the last call, then we now have to
   *   clear the command from the buffer.
   */
  buglog(("net_select  {"));
  while(1)  {
    tmp_read = readSet;
    tmp_write = writeSet;
    timer.tv_usec = 0;
    if (moreWork) timer.tv_sec = 0; /* Poll. */
    else timer.tv_sec = 1; /* Sit and wait for data. */

    TIME0;
    flushWrites();
    TIME1("flushWrites()", 0.005);
    selected = select(net_fd_top+1, &tmp_read, &tmp_write, NULL, &timer);
    if (selected == -1)  {
      switch(errno) {
      case EBADF:
        Logit("EBADF error from select ---");
        abort();
      case EINTR:
        Logit("select interrupted by signal, continuing ---");
        continue;
      case EINVAL:
        Logit("select returned EINVAL status ---");
        abort();
      case EPIPE:
      default:
        Logit("Select: error(%d):%s ---", errno, strerror(errno) );
        continue;
      }
    }

    /* Before every loop we clear moreWork. Then if we find a conn with more
     *   than one command in its input buffer, we process that command, then
     *   set moreWork to indicate that even after this work loop is done we
     *   have more work to do.
     */
    moreWork = 0;
    elapsed = refetch_ticker();
    if (elapsed)  {
      TIME0;
      if (process_heartbeat(&fd) == COM_LOGOUT)  {
        process_disconnection(fd);
        FD_CLR(fd, &tmp_read);
        FD_CLR(fd, &tmp_write);
      }
      TIME1("process_heartbeat()", 0.05);
    }
    for (fd = 0;  fd < net_fd_top+1;  fd++)  {
      if (netarray[fd].in_end)  {
        buglog(("%3d: Command \"%s\" left in buf", i, netarray[fd].in_buff));
        if (process_input(fd, netarray[fd].in_buff) == COM_LOGOUT)  {
          process_disconnection(fd);
          continue;
        }
        if (clearCmd(fd) == -1)  {
          buglog(("%3d: Closing", fd));
          process_disconnection(fd);
          continue;
        } 
        if (netarray[fd].in_end) { moreWork = 1; }
        /* continue; */
      }
      if (FD_ISSET(fd, &tmp_read))  {
        switch (netarray[fd].netstate) {
        case NETSTATE_LISTENING: /* A new inbound connection. */
          TIME0;
          newConn = do_accept(fd);
          TIME1("Do_accept()", 0.001);
          buglog(("%d: New conn", newConn));
          if (newConn >= 0)  {
            TIME0;
            process_new_connection(newConn, net_connectedHost(newConn));
            TIME1("process_new_connection()", 0.002);
          }
          continue;
        case NETSTATE_CONNECTED: /* New incoming data. */
          buglog(("%d: Ready for read", fd));
          assert(!netarray[fd].is_full);
          if (do_read(fd) == -1)  {
            buglog(("%d: Closed", fd));
            netarray[fd].netstate = NETSTATE_DISCONNECTED;
            process_disconnection(fd);
            continue;
          }
          if (netarray[fd].in_end)  {
            if (process_input(fd, netarray[fd].in_buff) == COM_LOGOUT)  {
              process_disconnection(fd);
              continue;
            }
            if (clearCmd(fd) == -1)  {
              buglog(("%3d: Closing", fd));
              process_disconnection(fd);
              continue;
            }
            if (netarray[fd].in_end) { moreWork = 1; }
            /* continue; */
          }
          break;
        case NETSTATE_UDP: /* A datagram from a web-interface. */
          /* Logit("Got UDP Message on %d", fd); */
	  /* rc = */ do_read_udp(fd);
	  continue;
	default:	/* It is not connected. */
          assert(!FD_ISSET(fd, &readSet)); /* bogus */
	  continue;
        }
      } /* fd_isset */
/* [PEM]: Testing... */
/* No new connection, nothing to read, try to flush output. */
      if (netarray[fd].netstate == NETSTATE_CONNECTED
         && netarray[fd].out_used > 0) {
        if (do_write(fd) < 0) {
          netarray[fd].netstate = NETSTATE_DISCONNECTED;
          process_disconnection(fd);
          continue;
        }
      }
    } /* for */
  } /* while(1) */
}


static int  do_read_udp(int fd)
{
  int rlen, wlen;
  unsigned alen;
  struct sockaddr_in addr;

  alen = sizeof addr;

  rlen = recvfrom(fd, netarray[fd].in_buff, sizeof netarray[fd].in_buff
  , 0 , (struct sockaddr *) &addr, &alen
  );
	/* We could check the ip address here ... */
  if (rlen >= (int) sizeof netarray[fd].in_buff) rlen = sizeof netarray[fd].in_buff;
  if (rlen <= 0) return 0;
  netarray[fd].fromHost = ntohl(addr.sin_addr.s_addr);
  netarray[fd].fromport = ntohs(addr.sin_port);
  while( rlen-- > 0) {
    switch(netarray[fd].in_buff[rlen]) {
    case ' ': case '\t':
    case '\n': case '\r': continue;
      }
    rlen++; break;
    }
  /* Logit("Read UDP Message %d", rlen); */
  if (rlen < 0) return 0;
  netarray[fd].in_buff[rlen] = 0;
  rlen = udp_command(udpbuff, sizeof udpbuff, netarray[fd].in_buff);
  if (rlen < 0) return 0;

  while(1) {
    wlen = sendto(fd ,udpbuff, rlen, 0 , (struct sockaddr *) &addr, alen);
    if (wlen == rlen) break;
    if (errno == EINTR) continue;
    return -1;
  }
  /* Logit("Wrote UDP Message %d", wlen); */
  return wlen;
}

static int  do_accept(int listenFd)
{
  int  newFd;
  struct sockaddr_in  addr;
  unsigned  addrLen = sizeof addr;

  newFd = accept(listenFd, (struct sockaddr *)&addr, &addrLen);
  if (newFd < 0)
    return newFd;
  if (newFd >= COUNTOF(netarray))  {
    close(newFd);
    return -1;
  }
  assert(netarray[newFd].netstate == NETSTATE_EMPTY);
  set_nonblocking(newFd);

  fd_init(newFd);
  netarray[newFd].fromHost = ntohl(addr.sin_addr.s_addr);
  netarray[newFd].fromport = ntohs(addr.sin_port);

  /* Logit("New connection on fd %d ---", newFd); */
  return newFd;
}


static void  fd_init(int fd)
{

  netarray[fd].netstate = NETSTATE_CONNECTED;
  netarray[fd].telnetState = 0;
  netarray[fd].is_full = 0;
  netarray[fd].is_throttled = 0;
  netarray[fd].fromHost = 0;
  netarray[fd].fromport = 0;

  netarray[fd].in_used = 0;
  netarray[fd].in_end = 0;
  netarray[fd].parse_src = 0;
  netarray[fd].parse_dst = 0;

  netarray[fd].out_size = NET_OUTBUFSZ;
  netarray[fd].out_used = 0;
  netarray[fd].out_buff = malloc(NET_OUTBUFSZ);
  if (fd > net_fd_top) net_fd_top = fd;
  FD_SET(fd, &readSet);
}


static void  fd_cleanup(int fd)
{

  if (netarray[fd].netstate == NETSTATE_EMPTY) {
    Logit("Fd_cleanup(%d) already empty, top fd(%d)", fd, net_fd_top);
    return;
  }
  netarray[fd].netstate = NETSTATE_EMPTY;
  FD_CLR(fd, &readSet);
  FD_CLR(fd, &writeSet);

  if (fd > net_fd_top) net_fd_top = fd;

  for ( ; net_fd_top >= 0; net_fd_top--) {
    if (netarray[net_fd_top].netstate != NETSTATE_EMPTY) break;
    FD_CLR(net_fd_top, &readSet);
    FD_CLR(net_fd_top, &writeSet);
  }
}


static void  flushWrites(void)
{
  int  fd;

  for (fd = 0;  fd < COUNTOF(netarray);  fd++)  {
    if (netarray[fd].netstate != NETSTATE_CONNECTED) continue;
    if (netarray[fd].out_used <= 0) continue;
    if (do_write(fd) < 0)  {
      buglog(("%3d: Write failed.", fd));
      netarray[fd].out_used = 0;
    }
  }
}


static int  do_write(int fd)
{
  int  written;
  struct netstruct  *conn = &netarray[fd];

  assert(conn->netstate == NETSTATE_CONNECTED);
  assert(conn->out_used > 0);
  written = write(fd, conn->out_buff, conn->out_used);
  if (written < 0) {
    switch (errno) {
    case EAGAIN:
      written = 0;
      break;
    case EPIPE:
    default:
      return -1;
    }
  }
 
  bytes_sent += written;
  if (written == (int)conn->out_used)  {
    conn->out_used = 0;
    FD_CLR(fd, &writeSet);
    if (conn->is_throttled)  {
      conn->is_throttled = 0;
      if (!conn->is_full) FD_SET(fd, &readSet);
    }
  } else  {
#if 1
    Logit("do_write(): Wrote only %d of %d bytes to fd %d.",
      written, conn->out_used, fd);
#endif
    /*
     * This memmove is costly, but on ra (where NNGS runs) the TCP/IP
     *   sockets have 60K buffers so this should only happen when netlag
     *   is completely choking you, in which case it will happen like once
     *   and then never again since your written will be 0 until the netlag
     *   ends.  I pity the fool who has netlag this bad and keep playing!
     */
    if (written > 0 && written < (int) conn->out_used)
      memmove(conn->out_buff, conn->out_buff+written, conn->out_used-written);
    conn->out_used -= written;
    if (!conn->is_throttled)  {
      conn->is_throttled = 1;
      FD_CLR(fd, &readSet);
    }
  }
  return 0;
}


static int  clearCmd(int fd)
{
  struct netstruct  *conn = &netarray[fd];

  if (conn->netstate != NETSTATE_CONNECTED) return 0;
  assert(conn->in_end);
  assert(conn->in_end <= conn->in_used);
  if (conn->in_end < conn->in_used)
    memmove(conn->in_buff, conn->in_buff + conn->in_end, conn->in_used - conn->in_end);
  conn->parse_dst -= conn->in_end;
  conn->parse_src -= conn->in_end;
  conn->in_used -= conn->in_end;
  conn->in_end = 0;
  if (checkForCmd(fd) == -1) return -1;
  if (conn->is_full)  {
    conn->is_full = 0;
    if (!conn->is_throttled)  FD_SET(fd, &readSet);
  }
  return 0;
}


static int  checkForCmd(int fd)
{
  struct netstruct  *conn = &netarray[fd];
  unsigned idx;
  unsigned char uc;
  char  *dst, *src;

  dst = conn->in_buff + conn->parse_dst;
  src = conn->in_buff + conn->parse_src;
  for (idx = conn->parse_src;  idx < conn->in_used;  ++idx, ++src) {
    uc = *(unsigned char*) src;
    switch (conn->telnetState) {
    case 0:                     /* Haven't skipped over any control chars or
                                   telnet commands */
      switch (uc) {
      case IAC:
        conn->telnetState = 1;
        break;
      case '\n': case '\r': case '\004':
        *dst = '\0';
        for( ++idx; idx < conn->in_used; idx++) {
          if (conn->in_buff[idx] == '\n') continue;
          if (conn->in_buff[idx] == '\r') continue;
          break;
        }
        conn->in_end = idx;
        conn->telnetState = 0;
        conn->parse_src = idx;
        conn->parse_dst = idx;
        return 0;
      default:
        if (!isprint(uc) && uc <= 127) /* Skip controls, keep high ascii */
        conn->telnetState = 0;
        else *(dst++) = uc;
        break;
      }
      break;
    case 1:                /* got telnet IAC */
      *src = '\n';
      switch(uc) {
      case IAC:		/* double IAC := quoted IAC */
        *(dst++) = uc;
        conn->telnetState = 0;
        break;
      case IP:		/* Interrupt Process */
        return -1;            /* ^C = logout */
      case DO:
        conn->telnetState = 4;
        break;
      case WILL: case DONT: case WONT:
        conn->telnetState = 3;   /* this is cheesy, but we aren't using em */
        break;
      case AYT:
        net_send(fd, (char*) ayt, sizeof ayt -1);
        conn->telnetState = 0;
        break;
      case EL:			/* erase line */
        dst = &conn->in_buff[0];
        conn->telnetState = 0;
        break;
      case GA:			/* Go Ahead */
      case EC:			/* Erase Character */
      case AO:			/* Abort Output */
      case BREAK:		/* Break on copper wire */
      case DM:			/* Data Mark */
      case NOP:			/* Nop */
      case SE:			/* Subnegotiation End */
      case EOR:			/* End Of Record */
      case ABORT:		/* Abort Process */
      case SUSP:		/* Suspend Process */
      case xEOF:		/* End Of File (but keep connection open) */
      default:                  /* dunno what it is, so ignore it */
        conn->telnetState = 0;
        break;
      }
      break;
    case 3:                     /* Option numbers for {WILL,DONT,WONT} */
      conn->telnetState = 0;
      break;
    case 4:                     /* got IAC DO, we only handle two opcodes */
      switch (uc) {
      case TELOPT_TM:	/* Timing mark */
        net_send(fd, (char *) will_tm, sizeof will_tm); break;
      case TELOPT_SGA:	/* Suppress GoAhead marks */
        net_send(fd, (char *) will_sga, sizeof will_sga); break;
      default: break;
      }
      conn->telnetState = 0;
      break;
    default:	/* unreacheable state, hopefully */
      assert(0);
    }
  }
  conn->parse_src = src - conn->in_buff;
  conn->parse_dst = dst - conn->in_buff;
  if (conn->in_used == sizeof conn->in_buff)  {
    conn->in_buff[sizeof conn->in_buff - 1] = '\0';
    conn->in_end = sizeof conn->in_buff;
  }
  return 0;
}


static int  do_read(int fd)
{
  int  readAmt;
  struct netstruct  *conn = &netarray[fd];

  if (conn->in_used > sizeof conn->in_buff) conn->in_used = sizeof conn->in_buff -1;
  assert(conn->netstate == NETSTATE_CONNECTED);
  assert(conn->in_used < sizeof conn->in_buff);
  readAmt = read(fd, conn->in_buff + conn->in_used, sizeof conn->in_buff - conn->in_used);
  if (readAmt == 0)  {
    return -1;
  }
  buglog(("    do_read(%d) read %d bytes\n", readAmt));
  if (readAmt < 0)  {
    switch (errno) {
    case  EAGAIN: return 0;
    default:
      /* net_close(fd); */
      return -1;
    }
  }
  bytes_received += readAmt;
  conn->in_used += readAmt;
  if (!conn->in_end) return checkForCmd(fd);
  return 0;
}

int  net_send(int fd, const char *src, int bufLen)
{
  int  i;
  struct netstruct  *net;

  if (fd == -1)
    return 0;
  assert((fd >= 0) && (fd < COUNTOF(netarray)));
  net = &netarray[fd];
  if (net->netstate != NETSTATE_CONNECTED) return 0;

  /* telnetify the output. */
  for (i = 0;  i < bufLen;  ++i)  {
    if (*src == '\n')		/* Network EOL is CRLF. */
      net->out_buff[net->out_used++] = '\r';
    net->out_buff[net->out_used++] = *(src++);

    if (net->out_used + 2 >= net->out_size)  {
      net->out_buff = realloc(net->out_buff, net->out_size *= 2);
    }
  }
  if (net->out_used > 0) {
    FD_SET(fd, &writeSet);
#if WRITE_AT_ONCE
    do_write(fd);		/* Try to get rid of it at once. */
#endif
  }
  return 0;
}


void  net_close(int fd)
{
  struct netstruct  *conn = &netarray[fd];

  if (fd < 0) return;
  if (conn->netstate == NETSTATE_EMPTY) return;
  if (conn->netstate == NETSTATE_CONNECTED
    && conn->out_used > 0)
    write(fd, conn->out_buff, conn->out_used);
  if (conffile.debug_general) Logit("Disconnecting fd %d ---", fd);
  if (conn->out_buff) free(conn->out_buff);
  conn->out_buff = NULL;
  conn->out_used = 0;
  conn->in_end = 0;
  close(fd);
  conn->netstate = NETSTATE_DISCONNECTED;
  fd_cleanup(fd);
}


void  net_closeAll(void)
{
  int  fd;

  for (fd = 0;  fd < COUNTOF(netarray);  ++fd)  {
    net_close(fd);
  }
}


int  net_connectedHost(int fd)
{
  return netarray[fd].fromHost;
}


void  net_echoOn(int fd)
{
  net_send(fd, (char *) wont_echo, sizeof wont_echo);
}


void  net_echoOff(int fd)
{
  net_send(fd, (char *) will_echo, sizeof will_echo);
}

int net_isalive(int fd) {

  if (fd < 0) return 0;
  switch(netarray[fd].netstate) {
  default:
  case NETSTATE_EMPTY: return 0;
  case NETSTATE_LISTENING: return 0;
  case NETSTATE_CONNECTED: return 1;
  case NETSTATE_DISCONNECTED: return 0;
  case NETSTATE_UDP: return 0;
  }
}

char * net_dumpslot(int fd)
{
char buff[400];
size_t pos;
int rc;
struct stat statje;

  if (fd < 0 || fd > net_fd_top) return NULL;

  rc = fstat(fd, &statje);
  if (!rc) rc = statje.st_mode;

  pos = sprintf(buff, "%d:%d:%x:%x:%x:%08x:%c:%c"
    , fd, netarray[fd].netstate, rc, netarray[fd].telnetState
    , netarray[fd].fromHost, netarray[fd].fromport
    , (netarray[fd].is_full) ? 'F' : '-'
    , (netarray[fd].is_throttled) ? 'T' : '-'
    );
  pos += sprintf(buff+pos, ":%u:%u:%p"
    , netarray[fd].out_used ,netarray[fd].out_size
    , netarray[fd].out_buff
    );
  pos += sprintf(buff+pos, ":%u:%u:%u:%u"
    , netarray[fd].in_used
    , netarray[fd].in_end
    , netarray[fd].parse_dst, netarray[fd].parse_src
    );
return statstr_dup(buff, pos);
}

