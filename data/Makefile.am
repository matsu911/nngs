#
# autotools subdirs
#
SUBDIRS=help ahelp

#
# local variables
#
MESSAGEDIRS=info ladder lists messages news spool
WITHSUBTREEDIRS=players cgames games stats/player_data
ONELETTER=a b c d e f g h i j k l m n o p q r s t u v w x y z

mydatadir=${prefix}/share/${PACKAGE}

# data files only can go into this category
mydata_DATA=emotestr

install-data-hook:
## This is from autobook 14.3 install-data-local:
install-data-hook:
	@for m in ${MESSAGEDIRS}; do \
	for f in $$m/[a-z]* ; do \
	  d="`echo $$f | sed -e 's,/[^/]*$$,,'`"; \
	  ${mkinstalldirs} ${DESTDIR}${mydatadir}/$$d; \
	  if test -f ${srcdir}/$$f; then p=${srcdir}/$$f; else p=$$f; fi; \
	  ${INSTALL_DATA} $$p ${DESTDIR}${mydatadir}/$$f; \
	  done \
	done
	@for d in ${WITHSUBTREEDIRS}; do \
	  ${mkinstalldirs} ${DESTDIR}${mydatadir}/$$d; \
	  for s in $(ONELETTER) ; do \
	    ${mkinstalldirs} ${DESTDIR}${mydatadir}/$$d/$$s; \
	  done \
	done

# Remove these files here... they are created locally
DISTCLEANFILES = *~ *%

@NO_WARNINGS@
