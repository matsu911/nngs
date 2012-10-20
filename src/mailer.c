/* mailer.c
 * Copyright (C) 2005 Adriaan W.D. van Kessel
 */

/*
    Part of:
    NNGS - The No Name Go Server
    Copyright (C) 1995-1996 Erik Van Riper (geek@nngs.cosmic.org)
    and John Tromp (tromp@daisy.uwaterloo.ca/tromp@cwi.nl)

    Adapted from:
    fics - An internet chess server.
    Copyright (C) 1993  Richard V. Nash

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

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef HAVE_CTYPE_H
#include <ctype.h>
#endif

#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif


#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

#include "missing.h"
#include "nngsconfig.h"
#include "conffile.h"
#include "nngsmain.h"
#include "utils.h"
#include "common.h"
#include "command.h"
#include "mailer.h"

#define WANT_DEBUG 0
#define WANTED_SMTP_PORT 25
#define WANT_MAIL_CHILD 1

#if WANT_MAIN
#define MAX_LINE_SIZE 1024
#endif

static int smtp_err = 0;
static void mail_tempnam(char *buff);
static int mail_one(const char *spool);
static int mail_child(const char *spool);
static int child_perror(const char *msg);
static int smtp_mail(FILE *fp, char *to, char *subj);
static int pars_addr(struct sockaddr_in *dst, const char * name, int port);
static int smtp_open(const char *mtaname, int port, const char *myname);
static int set_envelope(int fd, char *from, char *rcpt);
static int add_header(int fd, const char *name, const char *value);
static int set_data(int fd);
static int add_data(int fd, char *buff, int len);
static int do_ping_pong(int fd, char *buff, int len);
static int wrap_read(int fd, char *buff, int len);
static int wrap_write(int fd, char *buff, int len);
static int wrap_line(int fd, char *buff, int len);

FILE * popen(const char *path, const char *mode);

/* ---------------------------------------------------- */

int mail_spool(char *nbuff, const char *to, const char *subj, const char *text, const char *fname)
{
  char buff[MAX_LINE_SIZE];
  FILE *fp;

  if (!to) return -1;
  mail_tempnam(nbuff);
  fp = fopen(nbuff, "w");
  if (!fp) {
    Logit("Mail_spool(%s) : failed to open: %d(%s)"
    , buff, errno, strerror(errno));
    return -1;
  }
  fprintf(fp, "t %s\n", to);
  if (subj) fprintf(fp, "s %s\n", subj);
  fprintf(fp, "\n");
  if (text) fprintf(fp, "%s\n", text);
  if (fname) {
    FILE *in;
    in = fopen(fname, "r");
    if (!in) {
      Logit("Mail_spool(%s) : failed to open \"%s\": %d(%s)"
      , buff, fname, errno, strerror(errno));
      fclose(fp); return -2; }
    while (fgets(buff, sizeof buff, in)) fputs(buff,fp);
    fclose(in);
  }
  fclose(fp);
  return mail_child(nbuff);
}


static void mail_tempnam(char *buff)
{
  time_t now = globclock.time;
  static time_t then = 0;
  static int seq=0;
  char deet[40];
  int siz;

  if (now != then) seq = 0;
  sprintf(deet, time2str_file((time_t *) &now));
  siz = xyfilename(buff, FILENAME_SPOOL_sd, deet, seq);
  if (conffile.debug_mailer) Logit("Mail_tempnam() := [%d]%s", siz, buff);
  then = now; seq++;
  return;
}


static int mail_child(const char *spool)
{
  int rc, pid;

  if (conffile.want_mail_child > 0) {
    pid = fork();
    if (pid) {
      if (conffile.debug_mailer) Logit("Mail_child() =: %d", pid);
      return pid;
    }
    rc = mail_one(spool);
    if (conffile.debug_mailer) Logit("Mail_one() =: %d", rc);
    exit(0);
    return -1;
  } else {
    rc = mail_one(spool);
    if (conffile.debug_mailer) Logit("Mail_one() =: %d", rc);
    return rc;
  }
}

static int mail_one(const char *spool)
{
  FILE *fp, *pipo;
  char buff[MAX_LINE_SIZE];
  char *to=NULL;
  char *subj=NULL;
  int rc;

  fp = fopen(spool, "r");
  if (!fp) {
    Logit("Mail_one(%s) : failed to open: %d(%s)"
    , spool, errno, strerror(errno));
    return -1;
  }

  while(fgets(buff, sizeof buff, fp)) {
    switch(buff[0]) {
    case 't': to = mystrdup(buff+2); break;
    case 's': subj = mystrdup(buff+2); break;
    case '\0': case '\n': case ' ': goto body;
    default: continue;
  }}
body:

  if (!to) {
    Logit("Mail_one(%s) : no \"to:\"", spool);
    fclose(fp);
    if (subj) free(subj);
    return -2;}

  to[strlen(to)-1] = 0;
  if (subj) subj[strlen(subj)-1] = 0;

  if (memcmp(conffile.mail_program, "SMTP", 4)) {
    if (subj) sprintf(buff, "%s -s \"%s\" %s", conffile.mail_program, subj, to);
    else sprintf(buff, "%s %s", conffile.mail_program, to);
    if (conffile.debug_mailer) Logit("Mail_one(%s) : opening pipe: \"%s\""
       , spool, buff);
    pipo = popen(buff, "w");
    fprintf(pipo, "From: %s\n", conffile.server_email);
    if (conffile.smtp_reply_to) fprintf(pipo, "Reply-To: %s\n", conffile.smtp_reply_to);
    fprintf(pipo, "X-NNGS-SMTP: %s\n", "No, Sir!" );
    fprintf(pipo, "\n");
    while(fgets(buff, sizeof buff, fp)) fputs(buff, pipo);
    fclose(pipo);
    rc = 0;
  } else {
    rc =  smtp_mail(fp, to, subj);
    if (conffile.debug_mailer) Logit("Smtp_mail(to=%s,Subj=%s) returned %d"
       , to, subj, rc);
  }

  if (rc >= 0) unlink(spool);
  fclose(fp);
  if (to) free(to);
  if (subj) free(subj);
  return (rc>=0) ? 0: -1;
}

static int smtp_open(const char *mta, int port, const char *myname)
{
int fd = -1 , rc;
struct sockaddr_in addr;
char buff[1024];

if (!mta) mta = "localhost";
if (!myname) myname = "localhost";
	/* get host to connect to.
	** If this fails, we have nothing to do
	*/

smtp_err = 0;
rc = pars_addr(&addr , mta, port);
if (rc<0) { goto quit; }

fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
if (fd<0) { smtp_err = errno; goto quit; }

rc = connect(fd, (struct sockaddr*) &addr , sizeof addr) ;
if (rc < 0) { smtp_err = errno; goto quit; }

rc = do_ping_pong(fd, NULL, 0);
if (rc<0) {smtp_err = errno; goto quit; }

rc = sprintf(buff, "HELO %s\n", myname);
rc = do_ping_pong(fd, buff, rc);
if (rc<0) {smtp_err = errno; goto quit; }

quit:
if (smtp_err) {
	fprintf(stderr, "Failed connection to MTA=\"%s\" from \"%s\": %d (%s)\n"
	, mta, myname, smtp_err, strerror(smtp_err));
	close(fd);
	return -smtp_err;
	}
return fd;
}
/* -------------------------------------------------- */
static int set_envelope(int fd, char *from, char *rcpt)
{
int rc;
char buff[1024];

#if (WANT_DEBUG &4)
fprintf(stderr,"[Set_envelope(%s,%s)]\n", from , rcpt);
#endif

rc = sprintf(buff, "MAIL FROM:<%s>\n", from);
rc = do_ping_pong(fd, buff, rc);
if (rc < 0) {close(fd); return rc; }

rc = sprintf(buff, "RCPT TO:<%s>\n", rcpt);
rc = do_ping_pong(fd, buff, rc);

return rc;
}
/* -------------------------------------------------- */
static int set_data(int fd)
{
int rc;
char buff[1024];

#if (WANT_DEBUG &4)
fprintf(stderr,"[Set_data(%d)]\n", fd);
#endif

rc = sprintf(buff, "DATA\n");
rc = do_ping_pong(fd, buff, rc);
if (rc== -3) rc = 0;

return rc;
}
/* -------------------------------------------------- */
static int add_header(int fd, const char *name, const char *value)
{
int rc;
char buff[1024];

#if (WANT_DEBUG &4)
fprintf(stderr,"[Add_header(%s,%s)]\n", name?name :"Null", value?value:"Null");
#endif

if (name) rc = sprintf(buff, "%s: %s\n", name, value);
else memcpy(buff, "\n", rc = 1);
rc = wrap_line(fd, buff, rc);
if (rc < 0) {close(fd); return -1; }

return rc;
}
/* -------------------------------------------------- */
static int add_data(int fd, char *buff, int len)
{
int rc, idx;

if (!buff) {
	char pipo[] = ".\r\n\0";
	wrap_line(fd, NULL, 0); /* flush */
	return wrap_write(fd, pipo, 3); /* writable strings ... avoid . -> .. */
	}
if (len <= 0) len = strlen(buff);

for (idx=0; idx < len; idx += rc) {
	rc = wrap_line(fd, buff+idx, len - idx);
	if (rc < 0) {close(fd); return -1; }
	}

return idx;
}
/* -------------------------------------------------- */
static int wrap_line(int fd, char *buff, int len)
{
static int done =0, state = 0;
int rc=0, idx=0;
static char temp[512];

#if (WANT_DEBUG &2)
fprintf(stderr, " [Wrap_line(%d,%p,%d)]\n", fd, buff, len);
#endif

if (!buff) {
	len = 0;
	if (state) { memcpy(temp+done, "\r\n", 2); done += 2; }
	state = 0;
	}
else for (idx = 0; idx < len; idx++) {
	switch(buff[idx]) {
	case '.' :
		if (!state) temp[done++] = '.' ;
		state = 1; goto dodo;
	case '\r' :
		break;
	case '\n' :
		temp[done++] = '\r' ;
		state = 0; goto dodo;
	default:
		state = 1;
	dodo:
		temp[done++] = buff[idx];
		}
	if (done+2 < (int) sizeof temp) continue;
	rc = wrap_write(fd, temp, done);
#if (WANT_DEBUG &2)
	fprintf(stderr, " [Wrap_line(%d,%d) Loop :=%d,%d]\n", fd,len,done,rc);
#endif
	if (rc <=0) goto kut;
	if (rc < done) memmove(temp, temp + rc, done - rc);
	done -= rc;
	}

if (done)	{
	rc = wrap_write(fd, temp, done);
#if (WANT_DEBUG &2)
	fprintf(stderr, " [Wrap_line(%d,%d) Flush :=%d,%d]\n"
		, fd, len, done,rc);
#endif
	if (rc <=0) goto kut;
	if (rc < done) memmove(temp, temp + rc, done - rc);
	done -= rc;
	}
kut:
#if (WANT_DEBUG &2)
fprintf(stderr, " [Wrap_line(i=%d d=%d r=%d)]\n", idx, done, rc);
/* sleep(1); */

#endif

return rc <= 0 ? -1 :idx;
}
/* -------------------------------------------------- */
static int do_ping_pong(int fd, char *buff, int len)
{
int err=0,done;

char temp[1024] ;

#if (WANT_DEBUG &4)
fprintf(stderr,"[do_ping_pong(%s,%d)]\n", buff?buff:"NULL", len);
#endif
if (buff && len) {
	done = wrap_line(fd, buff, len);
	if (done < 0) {close(fd); goto quit; }
	}

for (err=done=0; 1 ; done += len) {
	len = wrap_read(fd, temp+done, sizeof temp -done);
	if (len <= 0) {close(fd); err = -1; goto quit; }
#if (WANT_DEBUG &2)
	temp[done+len] = 0;
	fprintf(stderr," [PartRead{%d+%d}{%02x|%02x}:\"%s\"]\n"
	, done,len, temp[3], temp[done+len-1], temp+done);
#endif
	if (done+len < 3) continue;
	if (temp[0] != '2') {err = '0' - temp[0]; break; }
	if (temp[done+len-1] == '\n') {
		if (temp[3] == '-') { done = 0; len = 0; continue; }
		else { break; }
		}
	else { done = 4; len = 0; }
	}

quit:
#if (WANT_DEBUG &4)
fprintf(stderr,"[Do_ping_pong(%s) err=%d done=%d] :\"%s\"]\n"
, buff, err, done, temp);
#endif
return (err) ? err : done;
}
/* -------------------------------------------------- */
static int wrap_read(int fd, char *buff, int len)
{
int rc;

while(1) {
	rc = read(fd, buff, len);
	if (rc < 0) switch(errno) {
		case EAGAIN:
		case EINTR: continue;
		default : 
			rc = errno;
			fprintf(stderr,"[Wrap_read(%d]: %d(%s)"
			, fd , rc, strerror(rc));
			rc = -1; goto quit;
		}
	if (rc >= len) rc--;
	buff[rc] = 0;
	break;
	}

quit:
#if (WANT_DEBUG &1)
fprintf(stderr,"  [Read(%d) := %d	\"%s\"]\n", fd, rc, buff);
#endif
return rc;
}
/* -------------------------------------------------- */
static int wrap_write(int fd, char *buff, int len)
{
int rc;

while(1) {
	rc = write(fd, buff, len);
	if (rc < 0) switch(errno) {
		case EAGAIN:
		case EINTR: continue;
		default : 
			rc = errno;
			fprintf(stderr,"[wrap_write(%d]: %d(%s)"
			, fd , rc, strerror(rc));
		rc = -1; goto quit;
		}
	break;
	}

quit:
#if (WANT_DEBUG &1)
if (rc >= 0 && rc < len) buff[rc] = 0; else buff[len-1] = 0;
fprintf(stderr, "  [Write(%d) := %d	\"%s\"]\n", fd, rc, buff);
#endif
return rc;
}
/* -------------------------------------------------- */
static int pars_addr(struct sockaddr_in *dst, const char * name, int port)
{
struct hostent *hp;

memset((char *)dst, 0, sizeof *dst);

dst->sin_family = AF_INET;
dst->sin_port = htons(port);

	/* this is ugly: avoid (DNS) hostnamelookup ... */
if (!name || !strcmp(name , "localhost")) {
	/* uint32_t ul; ul = htonl(0x7f000001);  Avoid c98 dependancy */
	char ul[4] = {0x7f,0,0,1};
	memcpy(&dst->sin_addr ,  &ul , sizeof ul);
	return 0;
	}

hp = gethostbyname(name);
if (!hp || hp->h_addrtype != AF_INET) {
	smtp_err = errno;
	return -1;
	}
#if 0
	{
	int ii,jj;
	unsigned char * cp;
	for(ii = 0;cp = hp->h_addr_list[ii] ; ii++)	{
		fprintf(stdout,"\n[%d]  ", ii);
		for(jj = 0;jj < hp->h_length ; jj++)	{
			fprintf(stdout,"%c%d", (jj) ? '.' : '=', *cp++);
			}
		}
	}
#endif
#if WANT_DUMP_SOCKET
	{
	char buffie[1024];
	cp = hp->h_addr_list[0];
	sprintf(buffie,"%d.%d.%d.%d"
		, (int) cp[0], (int) cp[1],(int) cp[2],(int) cp[3]);
	fprintf(stderr,"\nUsing[%s]->%s\n",name, buffie);
	}
#endif /* WANT_DUMP_SOCKET */
	memcpy(&dst->sin_addr,hp->h_addr_list[0],sizeof dst->sin_addr);

return 0;
}
/* ---------------------------------------------------- */

#if WANT_MAIN
int main()
{
int rc, fd;
char buffie[512] ;

fd = smtp_open(NULL, WANTED_SMTP_PORT, NULL);

if (fd < 0) goto kut;

set_envelope(fd, "nngs@localhost", "nngs@localhost");

rc = set_data(fd);
if (rc < 0) goto kut;
add_header(fd, "Subject", "Kuttje (met twee thee)");
add_header(fd, "Reply-To", "nngs@localhost");
add_header(fd, NULL, 0);

add_data(fd, "En een droge sherry, alstublieft!\n", 0);

while(fgets(buffie, sizeof buffie, stdin)) {
	rc = add_data(fd, buffie, 0);
	/* sleep(1); */
	if (rc < 0) break;
	}

add_data(fd, NULL, 0);

sleep(10);
close(fd);
kut:
exit(0);
}

#else

static int child_perror(const char *msg)
{
int err;

err = smtp_err;
if (!msg) msg = "Message";
Logit("Child_perror; %s : %d(%s)", msg, err, strerror(err));
return -1;
}

static int smtp_mail(FILE *fp, char *to, char *subj)
{
int rc, fd;
char buffie[1024] ;

fd = smtp_open(conffile.smtp_mta, conffile.smtp_portnum, conffile.smtp_helo);
if (fd < 0) { return child_perror("Smtp_open"); }

rc = set_envelope(fd, conffile.smtp_from, to);
if (rc < 0) { return child_perror("Smtp_envelope"); }

rc = set_data(fd);
if (rc < 0) { return child_perror("Smtp_data"); }
if (subj) add_header(fd, "Subject", subj);
if (conffile.smtp_reply_to) add_header(fd, "Reply-To", conffile.smtp_reply_to);
add_header(fd, "X-NNGS-SMTP", "Yes, Baby!" );
add_header(fd, NULL, 0);

while(fgets(buffie, sizeof buffie, fp)) {
	rc = add_data(fd, buffie, 0);
	if (rc < 0) { return child_perror("Smtp_loop"); }
	}

add_data(fd, NULL, 0);

close(fd);
return 0;
}

#endif

