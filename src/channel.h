/* channel.h
 *
 */

/*
    NNGS - The No Name Go Server
    Copyright (C) 1995-1997  Erik Van Riper (geek@midway.com)
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

#ifndef CHANNEL_H
#define CHANNEL_H

/* Set the number of channels (100+6 is plenty) */
/* AvK: NCHANNEL := normal channels, OCHANNEL := special purpose channels */
#define MAX_NCHANNEL 100
#define MAX_OCHANNEL 106
#define MAX_CHANNELS 106

#define YELL_STACK_SIZE 20

/* define the different "special" channel types */
#define CHANNEL_ASHOUT (MAX_NCHANNEL+0)  /* Admin shouts, for admins only */
#define CHANNEL_SHOUT  (MAX_NCHANNEL+1)  /* Regular shouts */
#define CHANNEL_GSHOUT (MAX_NCHANNEL+2)  /* GoBot shouts */
#define CHANNEL_LSHOUT (MAX_NCHANNEL+3)  /* Ladder shouts */
#define CHANNEL_LOGON  (MAX_NCHANNEL+4)  /* Player login announcments */
#define CHANNEL_GAME   (MAX_NCHANNEL+5)  /* Game announcments */

struct channel {
	unsigned is_special:1;	/* If this channel is an "special" channel */
	unsigned is_locked:1;	/* If the channel is locked to further people */
	unsigned is_hidden:1;	/* If the people inside are hidden from view */
	unsigned is_private:1;	/* If the people inside do not want outside yells. */
	char *ctitle;   /* The Channel title */
	char *Yell_Stack[YELL_STACK_SIZE]; /* Last channel yells */
	int Num_Yell;         /* Number of yells on stack */
	int count;
	int *members;
	} ;

struct channel carray[MAX_CHANNELS];

void channel_init(void);
int on_channel(int, int);
int channel_remove(int, int);
int channel_add(int, int);
int add_to_yell_stack(int, char *);

#endif /* CHANNEL_H */
