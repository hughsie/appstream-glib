# appdata-xml.m4
#
# serial 5

dnl APPDATA_XML
dnl Installs and validates AppData XML files.
dnl
dnl Call APPDATA_XML in configure.ac to check for the appdata-validate tool.
dnl Add @APPDATA_XML_RULES@ to a Makefile.am to substitute the make rules. Add
dnl .appdata.xml files to appdata_XML in Makefile.am and they will be validated
dnl at make check time, if appdata-validate is installed, as well as installed
dnl to the correct location automatically. Add --enable-appdata-validate to
dnl DISTCHECK_CONFIGURE_FLAGS in Makefile.am to require valid AppData XML when
dnl doing a distcheck.
dnl
dnl Adding files to appdata_XML does not distribute them automatically.

AU_DEFUN([APPDATA_XML],
[
  m4_pattern_allow([AM_V_GEN])
  AC_ARG_ENABLE([appdata-validate],
                [AS_HELP_STRING([--disable-appdata-validate],
                                [Disable validating AppData XML files during check phase])])

  AS_IF([test "x$enable_appdata_validate" != "xno"],
        [AC_PATH_PROG([APPSTREAM_UTIL], [appstream-util])
         AS_IF([test "x$APPSTREAM_UTIL" = "x"],
               [have_appdata_validate=no],
               [have_appdata_validate=yes
                AC_SUBST([APPSTREAM_UTIL])])],
        [have_appdata_validate=no])

  AS_IF([test "x$have_appdata_validate" != "xno"],
        [appdata_validate=yes],
        [appdata_validate=no
         AS_IF([test "x$enable_appdata_validate" = "xyes"],
               [AC_MSG_ERROR([AppData validation was requested but appstream-util was not found])])])

  AC_SUBST([appdataxmldir], [${datadir}/appdata])

  APPDATA_XML_RULES='
.PHONY : uninstall-appdata-xml install-appdata-xml clean-appdata-xml

mostlyclean-am: clean-appdata-xml

%.appdata.valid: %.appdata.xml
	$(AM_V_GEN) if test -f "$<"; then d=; else d="$(srcdir)/"; fi; \
		if test -n "$(APPSTREAM_UTIL)"; \
			then $(APPSTREAM_UTIL) --nonet validate $${d}$<; fi \
		&& touch [$]@

check-am: $(appdata_XML:.appdata.xml=.appdata.valid)
uninstall-am: uninstall-appdata-xml
install-data-am: install-appdata-xml

.SECONDARY: $(appdata_XML)

install-appdata-xml: $(appdata_XML)
	@$(NORMAL_INSTALL)
	if test -n "$^"; then \
		test -z "$(appdataxmldir)" || $(MKDIR_P) "$(DESTDIR)$(appdataxmldir)"; \
		$(INSTALL_DATA) $^ "$(DESTDIR)$(appdataxmldir)"; \
	fi

uninstall-appdata-xml:
	@$(NORMAL_UNINSTALL)
	@list='\''$(appdata_XML)'\''; test -n "$(appdataxmldir)" || list=; \
	files=`for p in $$list; do echo $$p; done | sed -e '\''s|^.*/||'\''`; \
	test -n "$$files" || exit 0; \
	echo " ( cd '\''$(DESTDIR)$(appdataxmldir)'\'' && rm -f" $$files ")"; \
	cd "$(DESTDIR)$(appdataxmldir)" && rm -f $$files

clean-appdata-xml:
	rm -f $(appdata_XML:.appdata.xml=.appdata.valid)
'
  _APPDATA_XML_SUBST(APPDATA_XML_RULES)
],
[Use the new APPSTREAM_XML macro instead of APPDATA_XML in configure.ac, and
 replace @APPDATA_XML_RULES@ with @APPSTREAM_XML_RULES@, appdata_XML with
 appstream_XML and --enable-appdata-validate with --enable-appstream-validate
 in Makefile.am])

dnl _APPDATA_XML_SUBST(VARIABLE)
dnl Abstract macro to do either _AM_SUBST_NOTMAKE or AC_SUBST
AC_DEFUN([_APPDATA_XML_SUBST],
[
AC_SUBST([$1])
m4_ifdef([_AM_SUBST_NOTMAKE], [_AM_SUBST_NOTMAKE([$1])])
]
)
