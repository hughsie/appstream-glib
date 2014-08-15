time client/appstream-builder						\
	--api-version=0.8						\
	--add-cache-id							\
	--old-metadata=./contrib/screenshots/fedora-21			\
	--log-dir=../createrepo_as_logs					\
	--temp-dir=./tmp/fedora-21					\
	--cache-dir=./contrib/cache					\
	--packages-dir=./contrib/packages/fedora-21/packages/		\
	--extra-appstream-dir=../fedora-appstream/appstream-extra	\
	--extra-appdata-dir=../fedora-appstream/appdata-extra		\
	--extra-screenshots-dir=../fedora-appstream/screenshots-extra	\
	--output-dir=./contrib/screenshots/fedora-21			\
	--screenshot-dir=./contrib/screenshots/fedora-21		\
	--basename=fedora-21						\
	--screenshot-uri=http://alt.fedoraproject.org/pub/alt/screenshots/f21/
./client/appstream-util non-package-yaml 				\
	./contrib/screenshots/fedora-21/fedora-21.xml.gz 				\
	./contrib/screenshots/fedora-21/applications-to-import.yaml
./client/appstream-util status-html 					\
	./contrib/screenshots/fedora-21/fedora-21.xml.gz 				\
	./contrib/screenshots/fedora-21/status.html
./client/appstream-util status-html 					\
	./contrib/screenshots/fedora-21/fedora-21-failed.xml.gz 			\
	./contrib/screenshots/fedora-21/failed.html

# sync the screenshots and metadata
cd contrib/screenshots/fedora-21/
./upload-screenshots.sh
cd -
