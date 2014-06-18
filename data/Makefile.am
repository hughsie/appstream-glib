SUBDIRS = 						\
	tests

man_MANS =
if ENABLE_MAN
man_MANS +=						\
	appstream-util.1				\
	appstream-builder.1
endif

XSLTPROC_FLAGS =					\
	--nonet						\
	--stringparam man.output.quietly 1		\
	--stringparam funcsynopsis.style ansi		\
	--stringparam man.th.extra1.suppress 1		\
	--stringparam man.authors.section.enabled 0	\
	--stringparam man.copyright.section.enabled 0

appstream-util.1: appstream-util.xml
	$(AM_V_GEN) xsltproc $(XSLTPROC_FLAGS) http://docbook.sourceforge.net/release/xsl/current/manpages/docbook.xsl $<
appstream-builder.1: appstream-builder.xml
	$(AM_V_GEN) xsltproc $(XSLTPROC_FLAGS) http://docbook.sourceforge.net/release/xsl/current/manpages/docbook.xsl $<

bashcompletiondir = @bashcompletiondir@
dist_bashcompletion_DATA = appstream-util appstream-builder

EXTRA_DIST =						\
	appstream-util.xml				\
	appstream-builder.xml

clean-local:
	rm -f *.1
	rm -f manpage.*

CLEANFILES =						\
	appstream-*.1					\
	manpage.*

-include $(top_srcdir)/git.mk
