dunst_depends = [
	cairo,
	glib,
	gio,
	gdk_pixbuf,
	pangocairo,
	systemd,
	libnotify,
	realtime,
	math,
]

dunst_src_files = files(
	'dbus.c', 
	'draw.c', 
	'dunst.c', 
	'icon-lookup.c', 
	'icon.c', 
	'ini.c', 
	'input.c', 
	'log.c', 
	'markup.c', 
	'menu.c', 
	'notification.c', 
	'option_parser.c', 
	'output.c', 
	'queues.c', 
	'rules.c', 
	'settings.c', 
	'utils.c', 
)

if x11_support
	add_global_arguments('-DENABLE_X11', language: 'c')

	dunst_depends += [
		x11,
		xinerama,
		xext,
		xrandr,
		xscrnsaver,
	]
	dunst_src_files += files(
		'x11/screen.c', 
		'x11/x.c', 
	)
endif

if wayland_support
	add_global_arguments('-DENABLE_WAYLAND', language: 'c')

	subdir('wayland/protocols')

	dunst_depends += [
		wayland_client, wayland_cursor,
	]
	dunst_src_files += files(
		'wayland/foreign_toplevel.c', 
		'wayland/libgwater-wayland.c', 
		'wayland/pool-buffer.c', 
		'wayland/wl.c', 
		'wayland/wl_output.c',
		'wayland/wl_seat.c', 
	)
endif

executable(
	'dunst',
	'../main.c',
	dunst_src_files,
	dependencies: dunst_depends,
	c_args: c_version_arg,
	install: true,
)
