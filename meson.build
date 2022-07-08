project(
  'i-spy-notify', 'c'
)

executable(
  'i-spy-notify',
  'src/main.c',
  dependencies: [
    dependency('dbus-1'),
    dependency('gtk+-3.0'),
    dependency('json-c'),
  ],
  sources: [
    'src/debug.c',
    'src/handler.c',
    'src/message.c',
  ],
  install: true,
)

install_man('doc/i-spy-notify.1')
install_data(sources: 'i-spy-notify.desktop', install_dir: 'share/applications')
install_data(sources: 'i-spy-notify.service', install_dir: 'lib/systemd/user')
install_data(sources: [
  'doc/examples/simple.json'
], install_dir: 'share/doc/i-spy-notify/examples')
