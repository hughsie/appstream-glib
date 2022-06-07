as_util_cargs = ['-DG_LOG_DOMAIN="As"']

if get_option('builder')
  appstream_builder = executable(
    'appstream-builder',
    sources : 'as-builder.c',
    include_directories : [
      include_directories('..'),
      asglib_incdir,
      asbuilder_incdir,
    ],
    dependencies : [
      gio,
      gdkpixbuf,
      libcurl,
      libarchive
    ],
    link_with : [
      asglib,
      asbuilder,
    ],
    c_args : as_util_cargs,
    install: true,
    install_dir : get_option('bindir'),
  )
endif

appstream_util = executable(
  'appstream-util',
  sources : 'as-util.c',
  include_directories : [
    include_directories('..'),
    asglib_incdir,
  ],
  dependencies : [
    gio,
    gdkpixbuf,
    libcurl,
    libarchive,
  ],
  link_with : asglib,
  c_args : as_util_cargs,
  install: true,
  install_dir : get_option('bindir'),
)

appstream_compose = executable(
  'appstream-compose',
  sources : 'as-compose.c',
  include_directories : [
    include_directories('..'),
    asglib_incdir,
  ],
  dependencies : [
    gio,
    gdkpixbuf,
    libcurl,
    libarchive,
  ],
  link_with : asglib,
  c_args : as_util_cargs,
  install: true,
  install_dir : get_option('bindir'),
)
