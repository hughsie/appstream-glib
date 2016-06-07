test_files =						\
	alpha-both.png					\
	alpha-horiz.png					\
	alpha-internal1.png				\
	alpha-internal2.png				\
	alpha-vert.png					\
	app-1-1.fc21.i686.rpm				\
	app-1-1.fc21.x86_64.rpm				\
	app-console-1-1.fc21.noarch.rpm			\
	app-extra-1-1.fc21.noarch.rpm			\
	appstream.xml					\
	asb-self-test.xml.gz				\
	broken.appdata.xml				\
	broken.metainfo.xml				\
	composite-1-1.fc21.x86_64.rpm			\
	example.appdata.xml				\
	example.desktop					\
	example.inf					\
	example.metainfo.xml				\
	example-v04.xml.gz				\
	example-v06.yml.gz				\
	firmware/2_0_0/firmware.bin			\
	firmware/2_0_0/firmware.inf			\
	firmware/2_0_1/firmware.bin			\
	firmware/2_0_1/firmware.inf			\
	firmware/2_0_1/firmware.metainfo.xml		\
	firmware/2_0_2/firmware.bin			\
	firmware/2_0_2/firmware.inf			\
	firmware/2_0_2/firmware.metainfo.xml		\
	flatpak_remote-name.xml				\
	font-1-1.fc21.noarch.rpm			\
	font-serif-1-1.fc21.noarch.rpm			\
	intltool.appdata.xml.in				\
	origin-icons.tar.gz				\
	origin.xml					\
	rpmbuild/app.png				\
	settings-panel.desktop				\
	screenshot.png					\
	ss-small.png					\
	success.appdata.xml				\
	test-0.1-1.fc21.noarch.rpm			\
	translated.appdata.xml				\
	usr/share/appdata/broken.appdata.xml		\
	usr/share/app-info/icons/aequorea/iceweasel.png	\
	usr/share/app-info/yaml/aequorea.yml		\
	usr/share/applications/broken.desktop		\
	usr/share/applications/test.desktop		\
	usr/share/icons/hicolor/64x64/apps/test2.png	\
	usr/share/icons/hicolor/128x128/apps/test3.png	\
	usr/share/kdeapp/translations/kdeapp_fr.qm	\
	usr/share/locale/en_GB/LC_MESSAGES/app.mo	\
	usr/share/locale/ru/LC_MESSAGES/app.mo		\
	usr/share/pixmaps/test.png			\
	validate.xml.gz

if HAVE_BUILDER
if HAVE_GCAB
colorhug-als-2.0.0.cab: $(srcdir)/firmware/2_0_0/firmware.*
	$(AM_V_GEN) touch -c -m -d"2000-01-01T00:00:00" $?;	\
	$(GCAB) --create --nopath $@			\
		$(srcdir)/firmware/2_0_0/firmware.bin	\
		$(srcdir)/firmware/2_0_0/firmware.inf
colorhug-als-2.0.1.cab: $(srcdir)/firmware/2_0_1/firmware.*
	$(AM_V_GEN) touch -c -m -d"2000-01-01T00:00:00" $?;	\
	$(GCAB) --create --nopath $@			\
		$(srcdir)/firmware/2_0_1/firmware.bin	\
		$(srcdir)/firmware/2_0_1/firmware.inf	\
		$(srcdir)/firmware/2_0_1/firmware.metainfo.xml
colorhug-als-2.0.2.cab: $(srcdir)/firmware/2_0_2/firmware.*
	$(AM_V_GEN) touch -c -m -d"2000-01-01T00:00:00" $?;	\
	$(GCAB) --create --nopath $@			\
		$(srcdir)/firmware/2_0_2/firmware.bin	\
		$(srcdir)/firmware/2_0_2/firmware.inf	\
		$(srcdir)/firmware/2_0_2/firmware.metainfo.xml

BUILT_SOURCES =						\
	colorhug-als-2.0.0.cab				\
	colorhug-als-2.0.1.cab				\
	colorhug-als-2.0.2.cab
endif
endif

CLEANFILES =						\
	$(BUILT_SOURCES)				\
	rpmbuild/app.bin				\
	rpmbuild/en_GB.mo				\
	rpmbuild/ru.mo

EXTRA_DIST = $(test_files)

-include $(top_srcdir)/git.mk
