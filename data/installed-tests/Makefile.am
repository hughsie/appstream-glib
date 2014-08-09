substitutions =						\
	-e s,@bindir\@,$(bindir),g			\
	-e s,@datadir\@,$(datadir),g

%.test: %.test.in
	$(AM_V_GEN) sed $(substitutions) $< > $@.tmp && mv $@.tmp $@

testmetadir = $(datadir)/installed-tests/$(PACKAGE)
testmeta_DATA = appdata-validate.test destdir-check.test

CLEANFILES = $(testmeta_DATA)
EXTRA_DIST =						\
	appdata-validate.test.in			\
	destdir-check.test.in

-include $(top_srcdir)/git.mk
