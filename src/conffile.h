/* conffile.h
 * Copyright (C) 2005 Adriaan W.D. van Kessel
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

#ifndef CONFFILE_H
#define CONFFILE_H

struct confstuff {
  char *version_string;
  char *compile_date;
  char *compile_time;
	/* */
  char *chroot_dir;
  char *chroot_user;
  char *chroot_group;
	/* Identity */
  char *server_name;
  char *server_address;
  char *server_ports;
  char *server_http;
  char *server_email;
  char *geek_email;
  char *admin_name;
	/* mail/SMTP stuff */
  char *mail_program;
  char *smtp_from;
  char *smtp_reply_to;
  char *smtp_mta;
  int smtp_portnum;
  char *smtp_helo;
	/* Directories for various stuff */
  char *mess_dir;
  char *help_dir;
  char *comhelp_dir;
  char *info_dir;
  char *ahelp_dir;
  char *stats_dir;
  char *config_dir;
  char *player_dir;
  char *game_dir;
  char *cgame_dir;
  char *problem_dir;
  char *board_dir;
  char *source_dir;
  char *lists_dir;
  char *news_dir;
  char *spool_dir;
	/* Some filenames */
  char *intergo_file;
  char *ratings_file;
  char *nratings_file;
  char *results_file;
  char *nresults_file;
  char *emotes_file;
  char *note_file;
  char *ladder9_file;
  char *ladder19_file;

  char *log_file;
  char *logons_file;

	/* results-files to communicate with mlrate */
  char *stats_games;
  char *stats_rgames;
  char *stats_cgames;
	/* Boolean flags */
  char allow_registration;
  char allow_unregistered;
  char allow_mail_to_localhost;
  char unregs_can_shout;
  char log_missing_files_on_read;
  char want_udp_port;
  char want_fork;
  char want_mail_child;
  char want_new_log_style;
	/* Misc stuff */
  char *def_prompt;
  float default_komi19;
  float default_komi13;
  float default_komi9;
  unsigned mode_for_dir;
  int game_save_frequency;
  int game_save_treshold;
  unsigned idle_check_interval;
  unsigned max_idle;
  unsigned max_login_idle;
  unsigned rating_read_interval;
  unsigned ladder_sift_interval;
	/* Debugging stuff */
  int debug_general;
  int debug_parray;
  int debug_mailer;
  };

#if CONFFILE_C
struct confstuff conffile = {NULL,NULL,NULL,NULL,};
#else
extern struct confstuff conffile;
#endif

int conf_file_read(const char * fname);
int conf_file_write(const char * fname);
int conf_file_fixup(void);

#endif /* CONFFILE_H */
