# Copyright (C) 2015 Igor Gnatenko <i.gnatenko.brain@gmail.com>
#
# Licensed under the GNU Lesser General Public License Version 2.1

asb_plugins_cargs = ['-DG_LOG_DOMAIN="Asb"']

asb_plugins = [
  ['absorb',       'asb-plugin-absorb.c',       []],
  ['gettext',      'asb-plugin-gettext.c',      []],
  ['hardcoded',    'asb-plugin-hardcoded.c',    []],
  ['desktop',      'asb-plugin-desktop.c',      []],
  ['appdata',      'asb-plugin-appdata.c',      []],
  ['font',         'asb-plugin-font.c',         [gdk, freetype, fontconfig]],
]

foreach i : asb_plugins
  shared_library(
    'libasb_plugin_@0@'.format(i.get(0)),
    sources : i.get(1),
    include_directories: [
      top_build_incdir,
      asbuilder_incdir,
      asglib_incdir,
    ],
    dependencies : [
      glib, gdkpixbuf, soup,
      i.get(2),
    ],
    link_with : asbuilder,
    c_args : asb_plugins_cargs
  )
endforeach
