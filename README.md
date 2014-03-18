AppStream-Glib
==============

This library provides GObjects and helper methods to make it easy to read and
write AppStream metadata. It also provides a simple DOM implementation that
makes it easy to edit nodes and convert to and from the standardized XML
representation.

What this library allows you to do:

 * Read and write compressed AppStream XML files
 * Add and search for applications in an application store
 * Get screenshot image data and release announcements
 * Easily retrieve the best application data for the current locale
 * Efficiently interface with more heavy-weight parsers like expat

For more information about what AppStream is, please see the wiki here:
http://www.freedesktop.org/wiki/Distributions/AppStream/

Getting Started
---------------

To install the libappstream-glib library you either need to install the
`libappstream-glib` package from your distributor, or you can build a local
copy. To do the latter just do:

    dnf install automake autoconf libtool glib-devel
    ./autogen.sh
    make
    make install

More Information
----------------

If you want to actually generate metadata rather than just consuming it, you
probably want to be looking at: https://github.com/hughsie/createrepo_as or if
you're completely lost, GNOME Software is a GUI tool that uses this library to
implement a software center. See `src/plugins/gs-plugin-appstream.c` if you
want some more examples on using this library where speed and latency really
matter.

Hacking
-------

If you want a new feature, or have found a bug or a way to crash this library,
please report as much information as you can to the issue tracker:
https://github.com/hughsie/appstream-glib/issues -- patches very welcome.

New functionality or crash fixes should include a test in `src/as-self-test.c`
to ensure we don't regress in the future. New functionality should also be
thread safe and also not leak *any* memory for success or failure cases.

License
----

LGPLv2+
