ahelpdatadir=${prefix}/share/${PACKAGE}/ahelp
ahelpdata_DATA=

LANGUAGES=en ch fr pl de


## This is from autobook 14.3 install-data-local:
install-data-hook:
	@for l in ${LANGUAGES}; do \
	for f in $$l/[a-z]* ; do \
	  d="`echo $$f | sed -e 's,/[^/]*$$,,'`"; \
	  $(mkinstalldirs) $(DESTDIR)$(ahelpdatadir)/$$d; \
	  if test -f $(srcdir)/$$f; then p=$(srcdir)/$$f; else p=$$f; fi; \
	  $(INSTALL_DATA) $$p $(DESTDIR)$(ahelpdatadir)/$$f; \
	  done \
	done

# Remove these files here... they are created locally
DISTCLEANFILES = *~

@NO_WARNINGS@
