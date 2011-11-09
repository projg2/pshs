ACLOCAL_AMFLAGS = -I m4

bin_PROGRAMS = pshs

pshs_SOURCES = src/main.c src/content-type.c src/content-type.h \
	src/index.c src/index.h src/handlers.c src/handlers.h \
	src/network.c src/network.h

AM_CPPFLAGS = $(LIBEVENT_CFLAGS)
LDADD = $(LIBEVENT_LIBS)

EXTRA_DIST = NEWS
NEWS: configure.ac Makefile.am
	git for-each-ref refs/tags --sort '-*committerdate' \
		--format '# %(tag) (%(*committerdate:short))%0a%(contents:body)' \
		> NEWS
