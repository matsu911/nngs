/* gameproc.h
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

#ifndef GAMEPROC_H
#define GAMEPROC_H

#include "command.h"

void game_ended(int, int, int);

#if WANT_PAIR
void process_move(int, char *, int);
int paired(int);
#else
void process_move(int, char *);
#endif /* WANT_PAIR */

int com_resign(int, struct parameter *);
int com_draw(int, struct parameter *);
int com_pause(int, struct parameter *);
int com_unpause(int, struct parameter *);
int com_abort(int, struct parameter *);
int com_games(int, struct parameter *);
int com_handicap(int, struct parameter *);
int com_observe(int, struct parameter *);
int com_allob(int, struct parameter *);
int com_moves(int, struct parameter *);
int com_gomoves(int, struct parameter *);
int com_mailmoves(int, struct parameter *);
int com_oldmoves(int, struct parameter *);
int com_mailoldm(int, struct parameter *);
int com_komi(int, struct parameter *);
int com_ladder(int, struct parameter *);
int com_load(int, struct parameter *);
int com_look(int, struct parameter *);
int com_touch(int, struct parameter *);
int com_stored(int, struct parameter *);
int com_sgf(int, struct parameter *);
int com_undo(int, struct parameter *);
int com_adjourn(int, struct parameter *);
int com_done(int, struct parameter *);
int com_history(int, struct parameter *);
int com_rhistory(int, struct parameter *);
int com_time(int, struct parameter *);
int com_title(int, struct parameter *);
int com_event(int, struct parameter *);
int com_status(int, struct parameter *);
int com_save(int, struct parameter *);
int com_pair(int, struct parameter *);
int com_ginfo(int, struct parameter *);
int com_sresign(int, struct parameter *);
int com_problem(int, struct parameter *);
int com_free(int, struct parameter *);
int com_unfree(int, struct parameter *);
int com_pteach(int, struct parameter *);

int com_moretime(int, struct parameter *);

#endif /* GAMEPROC_H */
