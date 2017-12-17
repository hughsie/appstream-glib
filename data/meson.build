subdir('installed-tests')
subdir('tests')

install_data('appdata-xml.m4', install_dir : 'share/aclocal')
install_data('appstream-xml.m4', install_dir : 'share/aclocal')
install_data('appstream-util', install_dir : 'share/bash-completion/completions')

if get_option('builder')
install_data('appstream-builder', install_dir : 'share/bash-completion/completions')
endif

install_data('appdata.its', install_dir : 'share/gettext/its')
install_data('appdata.loc', install_dir : 'share/gettext/its')

xsltproc = find_program('xsltproc', required : false)
if get_option('man') and xsltproc.found()
  custom_target('appstream-util-man',
    input: 'appstream-util.xml',
    output: 'appstream-util.1',
    install: true,
    install_dir: join_paths(get_option('mandir'), 'man1'),
    command: [
      xsltproc,
      '--nonet',
      '--stringparam', 'man.output.quietly', '1',
      '--stringparam', 'funcsynopsis.style', 'ansi',
      '--stringparam', 'man.th.extra1.suppress', '1',
      '--stringparam', 'man.authors.section.enabled', '0',
      '--stringparam', 'man.copyright.section.enabled', '0',
      '-o', '@OUTPUT@',
      'http://docbook.sourceforge.net/release/xsl/current/manpages/docbook.xsl',
      '@INPUT@'
    ]
  )
  custom_target('appstream-compose-man',
    input: 'appstream-compose.xml',
    output: 'appstream-compose.1',
    install: true,
    install_dir: join_paths(get_option('mandir'), 'man1'),
    command: [
      xsltproc,
      '--nonet',
      '--stringparam', 'man.output.quietly', '1',
      '--stringparam', 'funcsynopsis.style', 'ansi',
      '--stringparam', 'man.th.extra1.suppress', '1',
      '--stringparam', 'man.authors.section.enabled', '0',
      '--stringparam', 'man.copyright.section.enabled', '0',
      '-o', '@OUTPUT@',
      'http://docbook.sourceforge.net/release/xsl/current/manpages/docbook.xsl',
      '@INPUT@'
    ]
  )
  if get_option('builder')
    custom_target('appstream-builder-man',
      input: 'appstream-builder.xml',
      output: 'appstream-builder.1',
      install: true,
      install_dir: join_paths(get_option('mandir'), 'man1'),
      command: [
        xsltproc,
        '--nonet',
        '--stringparam', 'man.output.quietly', '1',
        '--stringparam', 'funcsynopsis.style', 'ansi',
        '--stringparam', 'man.th.extra1.suppress', '1',
        '--stringparam', 'man.authors.section.enabled', '0',
        '--stringparam', 'man.copyright.section.enabled', '0',
        '-o', '@OUTPUT@',
        'http://docbook.sourceforge.net/release/xsl/current/manpages/docbook.xsl',
        '@INPUT@'
      ]
    )
  endif
endif

