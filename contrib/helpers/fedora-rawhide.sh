time client/appstream-builder						\
	--api-version=0.8						\
	--log-dir=./contrib/logs/fedora-22				\
	--temp-dir=./tmp/fedora-22					\
	--cache-dir=./contrib/cache					\
	--packages-dir=./contrib/packages/fedora-rawhide/packages/	\
	--extra-appstream-dir=../fedora-appstream/appstream-extra	\
	--extra-appdata-dir=../fedora-appstream/appdata-extra		\
	--extra-screenshots-dir=../fedora-appstream/screenshots-extra	\
	--output-dir=./contrib/metadata					\
	--screenshot-dir=./contrib/screenshots/fedora-22		\
	--basename=fedora-22						\
	--screenshot-uri=http://alt.fedoraproject.org/pub/alt/screenshots/f22/
./client/appstream-util non-package-yaml 				\
	./contrib/metadata/fedora-22.xml.gz				\
	./contrib/screenshots/fedora-22/applications-to-import.yaml
./client/appstream-util status-html 					\
	./contrib/metadata/fedora-22.xml.gz				\
	./contrib/screenshots/fedora-22/status.html
./client/appstream-util status-html 					\
	./contrib/metadata/fedora-22-failed.xml.gz			\
	./contrib/screenshots/fedora-22/failed.html
