project(
	'dunst',
	'c',
	version: '1.9.2',
	license: 'MIT',
	meson_version: '>=0.60.0',
	default_options: [
		'c_std=gnu11',
		'warning_level=1',
		'b_ndebug=if-release',
	],
)

if get_option('buildtype').startswith('debug')
	add_project_arguments('-DDEBUG_BUILD', language: 'c')
endif

cc = meson.get_compiler('c')

cairo = dependency('cairo')
glib = dependency('glib-2.0')
gio = dependency('gio-2.0')
gdk_pixbuf = dependency('gdk-pixbuf-2.0')
pangocairo = dependency('pangocairo')
x11 = dependency('x11', required: get_option('x11'))
xinerama = dependency('xinerama', required: get_option('x11'))
xext = dependency('xext', required: get_option('x11'))
xrandr = dependency('xrandr', required: get_option('x11'), version: '>=1.5')
xscrnsaver = dependency('xscrnsaver', required: get_option('x11'))
systemd = dependency('systemd', required: get_option('systemd'))
libnotify = dependency('libnotify', required: get_option('dunstify'))
realtime = cc.find_library('rt')
math = cc.find_library('m')
wayland_client = dependency('wayland-client', required: get_option('wayland'))
wayland_protos = dependency('wayland-protocols', version: '>=1.12', required: get_option('wayland'))
wayland_cursor = dependency('wayland-cursor', required: get_option('wayland'))

x11_support = x11.found() and xinerama.found() and xext.found() and xrandr.found() and xscrnsaver.found()
wayland_support = wayland_client.found() and wayland_cursor.found() and wayland_protos.found()

if not x11_support and not wayland_support
	error('either wayland or x11 support is required')
endif

if wayland_support and not x11_support
	add_project_arguments('-DWAYLAND_ONLY', language: 'c')
endif

c_version_arg = '-DVERSION="@0@"'.format(meson.project_version())
sysconfdir = get_option('sysconfdir') / 'xdg'

add_project_arguments(
	'-DSYSCONFDIR="@0@"'.format(get_option('prefix') / sysconfdir),
	language: 'c'
)

subdir('src')

install_data('dunstctl', install_dir: get_option('bindir'))
install_data('dunstrc', install_dir: sysconfdir / 'dunst')

conf_data = configuration_data()
conf_data.set('bindir', get_option('bindir'))
conf_data.set('sysconfdir', sysconfdir)

configure_file(
	input: 'org.knopwob.dunst.service.in',
	output: 'dunst.service',
	configuration: conf_data,
	install_dir: get_option('datadir') / 'dbus-1/services',
)

if systemd.found()
	user_units_dir = systemd.get_variable(pkgconfig: 'systemduserunitdir')
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

subdir('test')

pod2man = find_program('pod2man', native: true, required: get_option('docs'))
if pod2man.found()
	subdir('docs')
endif

summary({
	'X11 support': x11_support,
	'Wayland support': wayland_support,
	'Man pages': pod2man.found(),
	'Dunstify': libnotify.found(),
	'Install systemd service units': systemd.found(),
}, bool_yn: true)
