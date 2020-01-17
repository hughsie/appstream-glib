asb_plugins_cargs = ['-DG_LOG_DOMAIN="Asb"']
name_suffix = []

if platform_osx
  name_suffix = 'so'
endif

shared_module(
  'asb_plugin_icon',
  sources : 'asb-plugin-icon.c',
  include_directories: [
    top_build_incdir,
    asbuilder_incdir,
    asglib_incdir,
  ],
  dependencies : [
    gdkpixbuf,
  ],
  c_args : asb_plugins_cargs,
  name_suffix: name_suffix,
  install : true,
  install_dir : plugindir,
)

shared_module(
  'asb_plugin_gettext',
  sources : 'asb-plugin-gettext.c',
  include_directories: [
    top_build_incdir,
    asbuilder_incdir,
    asglib_incdir,
  ],
  dependencies : [
    gdkpixbuf,
  ],
  c_args : asb_plugins_cargs,
  name_suffix: name_suffix,
  install : true,
  install_dir : plugindir,
)

shared_module(
  'asb_plugin_hardcoded',
  sources : 'asb-plugin-hardcoded.c',
  include_directories: [
    top_build_incdir,
    asbuilder_incdir,
    asglib_incdir,
  ],
  dependencies : [
    gdkpixbuf,
  ],
  c_args : asb_plugins_cargs,
  name_suffix: name_suffix,
  install : true,
  install_dir : plugindir,
)

shared_module(
  'asb_plugin_desktop',
  sources : 'asb-plugin-desktop.c',
  include_directories: [
    top_build_incdir,
    asbuilder_incdir,
    asglib_incdir,
  ],
  dependencies : [
    gdkpixbuf,
  ],
  c_args : asb_plugins_cargs,
  name_suffix: name_suffix,
  install : true,
  install_dir : plugindir,
)

shared_module(
  'asb_plugin_appdata',
  sources : 'asb-plugin-appdata.c',
  include_directories: [
    top_build_incdir,
    asbuilder_incdir,
    asglib_incdir,
  ],
  dependencies : [
    gdkpixbuf,
  ],
  c_args : asb_plugins_cargs,
  name_suffix: name_suffix,
  install : true,
  install_dir : plugindir,
)

shared_module(
  'asb_plugin_shell_extension',
  sources : 'asb-plugin-shell-extension.c',
  include_directories: [
    top_build_incdir,
    asbuilder_incdir,
    asglib_incdir,
  ],
  dependencies : [
    gdkpixbuf,
    json_glib,
  ],
  c_args : asb_plugins_cargs,
  name_suffix: name_suffix,
  install : true,
  install_dir : plugindir,
)

if get_option('fonts')
  shared_module(
    'asb_plugin_font',
    sources : 'asb-plugin-font.c',
    include_directories: [
      top_build_incdir,
      asbuilder_incdir,
      asglib_incdir,
    ],
    dependencies : [
      gdkpixbuf,
      gdk,
      freetype,
      fontconfig,
    ],
    c_args : asb_plugins_cargs,
    name_suffix: name_suffix,
    install : true,
    install_dir : plugindir,
  )
endif
