ACLOCAL_AMFLAGS = -I m4

bin_PROGRAMS = pshs

pshs_SOURCES = src/main.cxx src/content-type.cxx src/content-type.h \
	src/index.cxx src/index.h src/handlers.cxx src/handlers.h \
	src/network.cxx src/network.h src/rtnl.cxx src/qrencode.cxx \
	src/qrencode.h src/ssl.cxx src/ssl.h

AM_CXXFLAGS = -std=c++11
AM_CPPFLAGS = \
	$(LIBEVENT_CFLAGS) \
	$(QRENCODE_CFLAGS) \
	$(LIBSSL_CFLAGS) \
	$(LIBMNL_CFLAGS)
LDADD = \
	$(LIBEVENT_LIBS) \
	$(QRENCODE_LIBS) \
	$(LIBSSL_LIBS) \
	$(LIBMNL_LIBS)

EXTRA_DIST = NEWS
NEWS: configure.ac Makefile.am
	git for-each-ref refs/tags --sort '-*committerdate' \
		--format '# %(tag) (%(*committerdate:short))%0a%(contents:body)' \
		> NEWS
