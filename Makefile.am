ACLOCAL_AMFLAGS = -I m4

SUBDIRS = 						\
	libappstream-glib				\
	data						\
	docs						\
	po

if HAVE_BUILDER
SUBDIRS +=						\
	libappstream-builder
endif

# depends on libappstream-builder
SUBDIRS +=						\
	client

DISTCLEANFILES =					\
	appstream-glib-*.tar.xz

MAINTAINERCLEANFILES =					\
	*~						\
	ABOUT-NLS					\
	aclocal.m4					\
	ChangeLog					\
	compile						\
	config.guess					\
	config.h.*					\
	config.rpath					\
	config.sub					\
	configure					\
	depcomp						\
	gtk-doc.make					\
	INSTALL						\
	install-sh					\
	ltmain.sh					\
	Makefile.in					\
	missing						\
	mkinstalldirs					\
	po/*.header					\
	po/Makevars.template				\
	po/Rules-quot					\
	po/*.sed					\
	po/*.sin					\
	po/*.po~					\
	.zanata-cache					\
	test-driver

EXTRA_DIST =						\
	COPYING						\
	MAINTAINERS					\
	AUTHORS						\
	README.md					\
	NEWS						\
	autogen.sh					\
	config.h

distclean-local:
	if test $(srdcir) = .; then :; else \
		rm -f ChangeLog; \
	fi

ChangeLog:
	@echo Creating $@
	@if test -d "$(srcdir)/.git"; then \
	  (GIT_DIR=$(top_srcdir)/.git ./missing --run git log AS_0_0_X.. --stat -M -C --name-status  --date=short --no-color) | fmt --split-only > $@.tmp \
	  && mv -f $@.tmp $@ \
	  || ($(RM) $@.tmp; \
	      echo Failed to generate ChangeLog, your ChangeLog may be outdated >&2; \
	      (test -f $@ || echo git-log is required to generate this file >> $@)); \
	else \
	  test -f $@ || \
	  (echo A git checkout and git-log is required to generate ChangeLog >&2 && \
	  echo A git checkout and git-log is required to generate this file >> $@); \
	fi

DISTCHECK_CONFIGURE_FLAGS =				\
	--enable-dep11					\
	--enable-firmware				\
	--enable-fonts					\
	--enable-builder				\
	--with-bashcompletiondir=/${prefix}/share/bash-completion/completions

massif:
	make && \
	time ./client/appstream-util search gimp --help > /dev/null && \
	valgrind --massif-out-file=massif.out --threshold=0.1 --tool=massif \
		./client/.libs/lt-appstream-util search gimp --profile && \
	ms_print massif.out > massif.txt && \
	head -n 30 massif.txt

.PHONY: ChangeLog

-include $(top_srcdir)/git.mk
