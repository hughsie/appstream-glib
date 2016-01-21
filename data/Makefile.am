SUBDIRS = 						\
	installed-tests					\
	tests

m4datadir = $(datadir)/aclocal
dist_m4data_DATA =					\
	appstream-xml.m4

# ship this obsolete aclocal helper
dist_m4data_DATA +=					\
	appdata-xml.m4

man_MANS =
if ENABLE_MAN
man_MANS +=						\
	appstream-util.1				\
	appstream-compose.1
if HAVE_BUILDER
man_MANS +=						\
	appstream-builder.1
endif
endif

itsdatadir = $(datadir)/gettext/its
dist_itsdata_DATA = appdata.its appdata.loc

XSLTPROC_FLAGS =					\
	--nonet						\
	--stringparam man.output.quietly 1		\
	--stringparam funcsynopsis.style ansi		\
	--stringparam man.th.extra1.suppress 1		\
	--stringparam man.authors.section.enabled 0	\
	--stringparam man.copyright.section.enabled 0

appstream-util.1: appstream-util.xml
	$(AM_V_GEN) xsltproc $(XSLTPROC_FLAGS) http://docbook.sourceforge.net/release/xsl/current/manpages/docbook.xsl $<
appstream-compose.1: appstream-compose.xml
	$(AM_V_GEN) xsltproc $(XSLTPROC_FLAGS) http://docbook.sourceforge.net/release/xsl/current/manpages/docbook.xsl $<
if HAVE_BUILDER
appstream-builder.1: appstream-builder.xml
	$(AM_V_GEN) xsltproc $(XSLTPROC_FLAGS) http://docbook.sourceforge.net/release/xsl/current/manpages/docbook.xsl $<
endif
bashcompletiondir = @bashcompletiondir@
dist_bashcompletion_DATA = appstream-util
if HAVE_BUILDER
dist_bashcompletion_DATA += appstream-builder
endif

EXTRA_DIST =						\
	appstream-compose.xml				\
	appstream-util.xml				\
	appstream-builder.xml

clean-local:
	rm -f *.1
	rm -f manpage.*

CLEANFILES =						\
	appstream-*.1					\
	manpage.*

-include $(top_srcdir)/git.mk
