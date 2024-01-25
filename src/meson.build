dunst_depends = [
	cairo,
	glib,
	gio,
	gdk_pixbuf,
	pangocairo,
	x11,
	xinerama,
	xext,
	xrandr,
	xscrnsaver,
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
	'x11/screen.c', 
	'x11/x.c', 
)

if wayland_client.found() and wayland_cursor.found() and wayland_protos.found()
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
