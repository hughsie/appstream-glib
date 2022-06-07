AppStream-Glib
==============

This library provides GObjects and helper methods to make it easy to read and
write AppStream metadata. It also provides a simple DOM implementation that
makes it easy to edit nodes and convert to and from the standardized XML
representation. It also supports reading of Debian-style DEP-11 metadata.

What this library allows you to do:

 * Read and write compressed AppStream XML files
 * Read compressed Debian YAML files
 * Add and search for applications in an application store
 * Get screenshot image data and release announcements
 * Easily retrieve the best application data for the current locale
 * Efficiently interface with more heavy-weight parsers like expat

For more information about what AppStream is, please see the wiki here:
https://www.freedesktop.org/wiki/Distributions/AppStream/

Getting Started
---------------

To install the libappstream-glib library you either need to install the
`libappstream-glib` package from your distributor, or you can build a local
copy. To do the latter just do:

    dnf install docbook-utils gettext-devel glib-devel \
                gobject-introspection-devel gperf gtk-doc gtk3-devel \
                json-glib-devel libarchive-devel libcurl-devel \
                libstemmer-devel libuuid-devel libyaml-devel \
                meson rpm-devel
    mkdir build && cd build
    meson .. --prefix=/opt -Dbuilder=false
    ninja

Hacking
-------

If you want a new feature, or have found a bug or a way to crash this library,
please report as much information as you can to the issue tracker:
https://github.com/hughsie/appstream-glib/issues -- patches very welcome.

New functionality or crash fixes should include a test in `libappstream-builder/
as-self-test.c`
to ensure we don't regress in the future. New functionality should also be
thread safe and also not leak *any* memory for success or failure cases.

Translations
------------

Translations of the natural language strings are managed through a
third party translation interface, transifex.com.
Newly added strings will be periodically uploaded there for translation,
and any new translations will be merged back to the project source code.

Please use [https://www.transifex.com/projects/p/appstream-glib/](https://www.transifex.com/projects/p/appstream-glib/) to contribute translations,
rather than sending pull requests.

AppStream-Builder
=================

appstream-builder is a tool that allows us to create AppStream metadata from a
directory of packages.
It is typically used when generating distribution metadata, usually at the same
time as modifyrepo or createrepo.

What this tool does:

 * Searches a directory of packages and reads just the RPM header of each.
 * If a package contains an interesting file, just the relevant files are
   decompressed from the package archive.
 * A set of plugins are run on the extracted files, and if these match certain
   criteria `AsbApplication` objects are created.
 * Any screenshots referenced are downloaded to a local cache.
   This is optional and can be disabled with `--nonet`.
 * When all the packages are processed, some of the `AsbApplication` objects are
   merged into single applications. This is how fonts are collected.
 * The `AsbApplication` objects are serialized to XML and written to a
   compressed archive.
 * Any application icons or screenshots referenced are written to a .tar archive.

Getting Started
---------------

To run appstream-builder you either need to install the package containing the
binary and data files, or you can build a local copy. To do the latter just do:

    dnf install docbook-utils gettext-devel glib-devel \
                gobject-introspection-devel gperf gtk-doc gtk3-devel \
                libarchive-devel libsoup-devel \
                libstemmer-devel libuuid-devel libyaml-devel \
                meson rpm-devel rpm-devel
    mkdir build && cd build
    meson .. --prefix=/opt -Dbuilder=true
    ninja

To actually run the extractor you can do:

    ./client/appstream-builder --verbose   \
                      --max-threads=8 \
                      --log-dir=/tmp/logs \
                      --packages-dir=/mnt/archive/Megarpms/21/Packages \
                      --temp-dir=/mnt/ssd/AppStream/tmp \
                      --output-dir=./repodata \
                      --screenshot-url=http://megarpms.org/screenshots/ \
                      --basename="megarpms-21"

Note: it is possible to use "globs" like `/mnt/archive/Megarpms/21/Packages*` to
 match multiple directories or packages.

This will output a lot of progress text. Now, go and make a cup of tea and wait
patiently if you have a lot of packages to process. After this is complete
you should finally see:

    Writing ./repodata/megarpms-21.xml.gz
    Writing ./repodata/megarpms-21-failed.xml.gz
    Writing ./repodata/megarpms-21-ignore.xml.gz
    Writing ./repodata/megarpms-21-icons.tar
    Done!

You now have two choices what to do with these files. You can either upload
them with the rest of the metadata you ship (e.g. in the same directory as
`repomd.xml` and `primary.sqlite.bz2`) which will work with Fedora 22 and later:

    modifyrepo_c \
        --no-compress \
        /tmp/asb-md/appstream.xml.gz \
        /path/to/repodata/
    modifyrepo_c \
        --no-compress \
        /tmp/asb-md/appstream-icons.tar.gz \
        /path/to/repodata/

You can then do something like this in the megarpms-release.spec file:

    Source1:   http://www.megarpms.org/temp/megarpms-20.xml.gz
    Source2:   http://www.megarpms.org/temp/megarpms-20-icons.tar.gz

    %install
    DESTDIR=%{buildroot} appstream-util install %{SOURCE1} %{SOURCE2}

This ensures that gnome-software can access both data files when starting up.

What is an application
----------------------

Applications are defined in the context of AppStream as such:

 * Installs a desktop file and would be visible in a desktop
 * Has an metadata extractor (e.g. libappstream-builder/plugins/asb-plugin-gstre
amer.c)
   and includes an AppData file

Guidelines for applications
---------------------------

These guidelines explain how we filter applications from a package set.

First, some key words:
 * **SHOULD**: The application should do this if possible
 * **MUST**: The application or addon must do this to be included
 * **CANNOT**: the application or addon must not do this

The current rules of inclusion are thus:

 * Icons **MUST** be installed in `/usr/share/pixmaps/*`, `/usr/share/icons/*`,
   `/usr/share/icons/hicolor/*/apps/*`, or `/usr/share/${app_name}/icons/*`
 * Desktop files **MUST** be installed in `/usr/share/applications/`
   or `/usr/share/applications/kde4/`
 * Desktop files **MUST** have `Name`, `Comment` and `Icon` entries
 * Valid applications with `NoDisplay=true` **MUST** have an AppData file.
 * Applications with `Categories=Settings`, `Categories=ConsoleOnly` or
   `Categories=DesktopSettings` **MUST** have an AppData file.
 * Applications **MUST** have had an upstream release in the last 5 years or
   have an AppData file.
 * Application icon **MUST** be available in 48x48 or larger
 * Applications must have at least one main or additional category listed
   in the desktop file or supply an AppData file.
   See https://specifications.freedesktop.org/menu-spec/latest/apa.html and
   https://specifications.freedesktop.org/menu-spec/latest/apas02.html for the
   full `Categories` list.
 * Codecs **MUST** have an AppData file
 * Input methods **MUST** have an AppData file
 * If included, AppData files **MUST** be valid XML
 * AppData files **MUST** be installed into `/usr/share/metainfo`
 * Application icons **CANNOT** use XPM or ICO format
 * Applications **CANNOT** use obsolete toolkits such as GTK+-1.2 or QT3
 * Applications that ship a desktop file **SHOULD** include an AppData file.
 * Screenshots **SHOULD** be in 16:9 aspect ratio
 * Application icons **SHOULD** have an alpha channel
 * Applications **SHOULD** ship a 256x256 PNG format icon or scalable SVG
 * Applications **SHOULD** ship a matching High Contrast icon
 * Applications **SHOULD** not depend on other applications
 * AppData files **SHOULD** include translations
 * Desktop files **SHOULD** include translations

Guidelines for fonts
--------------------

 * Fonts **MUST** have a valid MetaInfo file installed to /usr/share/appdata
 * Fonts packaged in multiple packages **SHOULD** have multiple MetaInfo files
 * Fonts families **SHOULD** only have one description section
 * Fonts of different styles or weights of the same family **SHOULD** use `<exte
nds>`
 * MetaInfo files **SHOULD** include translations where possible

License
-------

LGPLv2+
