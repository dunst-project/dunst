project(
	'dunst',
	'c',
	version: '1.9.2',
	license: 'MIT',
	meson_version: '>=0.60.0',
	default_options: [
		'c_std=gnu99',
		'warning_level=1',
		'buildtype=release',
	],
)

add_project_arguments([
	'-pedantic',
	'-Wno-gnu-zero-variadic-macro-arguments',
	'-Wno-overlength-strings',
], language: 'c')

if get_option('buildtype').startswith('debug')
	add_project_arguments('-DDEBUG_BUILD', language: 'c')
endif

cc = meson.get_compiler('c')

cairo = dependency('cairo')
glib = dependency('glib-2.0')
gio = dependency('gio-2.0')
gdk_pixbuf = dependency('gdk-pixbuf-2.0')
pangocairo = dependency('pangocairo')
x11 = dependency('x11')
xinerama = dependency('xinerama')
xext = dependency('xext')
xrandr = dependency('xrandr', version: '>=1.5')
xscrnsaver = dependency('xscrnsaver')
systemd = dependency('systemd', required: get_option('systemd'))
libnotify = dependency('libnotify', required: get_option('dunstify'))
realtime = cc.find_library('rt')
math = cc.find_library('m')
wayland_client = dependency('wayland-client', required: get_option('wayland'))
wayland_protos = dependency('wayland-protocols', version: '>=1.32', required: get_option('wayland'))
wayland_cursor = dependency('wayland-cursor', required: get_option('wayland'))

c_version_arg = '-DVERSION="@0@"'.format(meson.project_version())

subdir('src')

install_data('dunstctl', install_dir: get_option('bindir'))
install_data('dunstrc', install_dir: get_option('sysconfdir') / 'dunst')

conf_data = configuration_data()
conf_data.set('bindir', get_option('bindir'))
conf_data.set('sysconfdir', get_option('sysconfdir'))

configure_file(
	input: 'org.knopwob.dunst.service.in',
	output: '@BASENAME@',
	configuration: conf_data,
	install_dir: get_option('datadir') / 'dbus-1/services',
)

if systemd.found()
	user_units_dir = systemd.get_pkgconfig_variable('systemduserunitdir')
	configure_file(
		configuration: conf_data,
		input: 'dunst.systemd.service.in',
		output: '@BASENAME@',
		install_dir: user_units_dir,
	)
endif

if libnotify.found()
	executable(
		'dunstify',
		'dunstify.c',
		dependencies: [ glib, libnotify, gdk_pixbuf ],
		install: true,
	)
endif

if get_option('test')
	subdir('test')
endif

if get_option('docs').enabled()
	subdir('docs')
endif
