substitutions = \
	-e s,@datadir\@,$(datadir),g

%.test: %.test.in
	$(AM_V_GEN) sed $(substitutions) $< > $@.tmp && mv $@.tmp $@

testmetadir = $(datadir)/installed-tests/$(PACKAGE)
testmeta_DATA = appdata-validate.test

CLEANFILES = appdata-validate.test
EXTRA_DIST = appdata-validate.test.in
