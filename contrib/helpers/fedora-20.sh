time client/appstream-builder						\
	--api-version=0.3						\
	--log-dir=./contrib/logs/fedora-20				\
	--temp-dir=./tmp/fedora-20					\
	--cache-dir=./contrib/cache					\
	--packages-dir=./contrib/packages/fedora-20/packages/		\
	--extra-appstream-dir=../fedora-appstream/appstream-extra	\
	--extra-appdata-dir=../fedora-appstream/appdata-extra		\
	--extra-screenshots-dir=../fedora-appstream/screenshots-extra	\
	--output-dir=./contrib/metadata					\
	--screenshot-dir=./contrib/screenshots/fedora-20		\
	--basename=fedora-20						\
	--screenshot-uri=http://alt.fedoraproject.org/pub/alt/screenshots/f20/
./client/appstream-util non-package-yaml 				\
	./contrib/metadata/fedora-20.xml.gz				\
	./contrib/screenshots/fedora-20/applications-to-import.yaml
./client/appstream-util status-html 					\
	./contrib/metadata/fedora-20.xml.gz				\
	./contrib/screenshots/fedora-20/status.html
./client/appstream-util status-html 					\
	./contrib/metadata/fedora-20-failed.xml.gz			\
	./contrib/screenshots/fedora-20/failed.html
