# -*- mode: python -*-

top = '.'

def options(opt):
    opt.load('compiler_c')

def configure(conf):
    conf.load('compiler_c')
    for header in ['time.h',
                   'sys/types.h',
                   'sys/stat.h',
                   'dirent.h',
                   'ctype.h',
                   'arpa/telnet.h',
                   'errno.h',
                   'fcntl.h',
                   'gdbm.h',
                   "inttypes.h",
                   "memory.h",
                   "ndbm.h",
                   "netdb.h",
                   "netinet/in.h",
                   "signal.h",
                   "stdint.h",
                   "stdio.h",
                   "stdlib.h",
                   "strings.h",
                   "string.h",
                   "sys/socket.h",
                   "sys/time.h",
                   "term.h",
                   "unistd.h",
                   "utime.h"]:
        conf.check(header_name=header, features='c cprogram')

    for fname, header in [('ftruncate', 'unistd.h'),
                          ('usleep', 'unistd.h'),
                          ('gettimeofday', 'sys/time.h'),
                          ('pclose', 'stdio.h'),
                          ('popen', 'stdio.h'),
                          ('random', 'stdlib.h'),
                          ('snprintf', 'stdio.h'),
                          ('vsnprintf', 'stdio.h')]:
        conf.check_cc(function_name=fname, header_name=header, mandatory=True) 

    conf.check_cc(lib='m', cflags='-Wall') 

    conf.check_cc(fragment='''
                        #include <stdio.h>
                        int main() { printf("%d", sizeof(int)); return 0; } ''',
                  define_name = "SIZEOF_INT",
                  execute     = True,
                  define_ret  = True,
                  msg         = "Checking for sizeof(int)") 
    conf.check_cc(fragment='''
                        #include <stdio.h>
                        int main() { printf("%d", sizeof(long)); return 0; } ''',
                  define_name = "SIZEOF_LONG",
                  execute     = True,
                  define_ret  = True,
                  msg         = "Checking for sizeof(long)") 

    conf.define("AHELP_DIR", "NONE/nngssrv/ahelp")
    conf.define("CGAME_DIR", "NONE/nngssrv/cgames")
    conf.define("EMOTES_FILE", "NONE/nngssrv/emotestr")
    conf.define("FIND_FILE", "NONE/nngssrv/find.out")
    conf.define("GAME_DIR", "NONE/nngssrv/games")
    conf.define("GNU_PACKAGE", "nngssrv")
    conf.define("HELP_DIR", "NONE/nngssrv/help")
    conf.define("INFO_DIR", "NONE/nngssrv/info")
    conf.define("INTERGO_FILE", "NONE/nngssrv/intergo/intergo")
    conf.define("LADDER19_FILE", "NONE/nngssrv/ladder/ladder19")
    conf.define("LADDER9_FILE", "NONE/nngssrv/ladder/ladder9")
    conf.define("LIST_DIR", "NONE/nngssrv/lists")
    conf.define("LOGONS_FILE", "NONE/nngssrv/stats/logons")
    conf.define("LOG_FILE", "NONE/nngssrv/stats/logfile")
    conf.define("MAILPROGRAM", "/usr/bin/mail")
    conf.define("MESSAGE_DIR", "NONE/nngssrv/messages")
    conf.define("MESS_AMOTD", "admotd")
    conf.define("MESS_LOGIN", "login")
    conf.define("MESS_LOGOUT", "logout")
    conf.define("MESS_MOTD", "motd")
    conf.define("MESS_UNREGISTERED", "unregistered")
    conf.define("MESS_WELCOME", "welcome")
    conf.define("NEWS_DIR", "NONE/nngssrv/news")
    conf.define("NOTE_FILE", "NONE/nngssrv/note")
    conf.define("NRATINGS_FILE", "NONE/nngssrv/ratdb")
    conf.define("NRESULTS_FILE", "NONE/nngssrv/nresults")
    conf.define("PACKAGE", "nngssrv")
    conf.define("PACKAGE_BUGREPORT", "")
    conf.define("PACKAGE_NAME", "")
    conf.define("PACKAGE_STRING", "")
    conf.define("PACKAGE_TARNAME", "")
    conf.define("PACKAGE_VERSION", "")
    conf.define("PLAYER_DIR", "NONE/nngssrv/players")
    conf.define("PROBLEM_DIR", "NONE/nngssrv/problems")
    conf.define("RATINGS_FILE", "NONE/nngssrv/results-rated")
    conf.define("RESULTS_FILE", "NONE/nngssrv/results")
    conf.define("SPOOL_DIR", "NONE/nngssrv/spool")
    conf.define("STATS_DIR", "NONE/nngssrv/stats")
    conf.define("STDC_HEADERS", 1)
    conf.define("TIME_WITH_SYS_TIME", 1)
    conf.define("VERSION", "1.1.22")
    conf.define("ulong", "unsigned long")

    conf.write_config_header('config.h')

    conf.define('HAVE_CONFIG_H', 1)

def build(bld):
    bld.stlib(source=bld.path.ant_glob('mlrate/src/*.c'),
              target='mlrate')
    bld.program(source=bld.path.ant_glob('src/*.c'),
                use='mlrate',
                includes='. build',
                target='nngsrv')
