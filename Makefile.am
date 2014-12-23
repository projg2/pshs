ACLOCAL_AMFLAGS = -I m4

bin_PROGRAMS = pshs

pshs_SOURCES = src/main.c src/content-type.c src/content-type.h \
	src/index.c src/index.h src/handlers.c src/handlers.h \
	src/network.c src/network.h src/rtnl.c src/qrencode.c \
	src/qrencode.h src/ssl.c src/ssl.h

AM_CPPFLAGS = $(LIBEVENT_CFLAGS) $(QRENCODE_CFLAGS) $(LIBSSL_CFLAGS)
LDADD = $(LIBEVENT_LIBS) $(QRENCODE_LIBS) $(LIBSSL_LIBS)

EXTRA_DIST = NEWS
NEWS: configure.ac Makefile.am
	git for-each-ref refs/tags --sort '-*committerdate' \
		--format '# %(tag) (%(*committerdate:short))%0a%(contents:body)' \
		> NEWS
