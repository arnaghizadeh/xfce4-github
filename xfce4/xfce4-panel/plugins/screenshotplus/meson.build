plugin_sources = [
  'screenshotplus.c',
  'screenshotplus.h',
]

plugin_install_subdir = 'xfce4' / 'panel' / 'plugins'

x11_dep = dependency('x11')

shared_module(
  'screenshotplus',
  plugin_sources,
  sources: xfce4_panel_resources_c,
  gnu_symbol_visibility: 'hidden',
  c_args: [
    '-DG_LOG_DOMAIN="@0@"'.format('libscreenshotplus'),
  ],
  include_directories: [
    include_directories('..' / '..'),
  ],
  dependencies: [
    gtk,
    libxfce4util,
    libxfce4ui,
    libxfce4windowing,
    xfconf,
    x11_dep,
  ],
  link_with: [
    libpanel_common,
    libxfce4panel,
  ],
  install: true,
  install_dir: get_option('prefix') / get_option('libdir') / plugin_install_subdir,
)

i18n.merge_file(
  input: configure_file(
    input: 'screenshotplus.desktop.in.in',
    output: 'screenshotplus.desktop.in',
    configuration: {
      'LIBXFCE4PANEL_VERSION_API': libxfce4panel_version_api,
    },
    install: false,
  ),
  output: 'screenshotplus.desktop',
  po_dir: '..' / '..' / 'po',
  type: 'desktop',
  install: true,
  install_dir: get_option('prefix') / get_option('datadir') / plugin_install_subdir,
)
