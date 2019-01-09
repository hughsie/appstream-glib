FROM fedora:29

RUN dnf -y update
RUN dnf -y install \
	docbook-style-xsl \
	docbook-utils \
	fontconfig-devel \
	freetype-devel \
	gdk-pixbuf2-devel \
	gettext \
	glib2-devel \
	gobject-introspection-devel \
	gperf \
	gtk3-devel \
	gtk-doc \
	json-glib-devel \
	libarchive-devel \
	libsoup-devel \
	libstemmer-devel \
	libuuid-devel \
	libxslt \
	libyaml-devel \
	meson \
	pango-devel \
	redhat-rpm-config \
	rpm-devel \
	sqlite-devel
RUN mkdir /build
WORKDIR /build
