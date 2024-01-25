wl_protocol_dir = wayland_protos.get_variable(pkgconfig: 'pkgdatadir')

wayland_scanner = dependency('wayland-scanner', version: '>=1.14.91', native: true)
wayland_scanner_path = wayland_scanner.get_variable(pkgconfig: 'wayland_scanner')
wayland_scanner_prog = find_program(wayland_scanner_path, native: true)

wayland_scanner_code = generator(
	wayland_scanner_prog,
	output: '@BASENAME@.h',
	arguments: ['private-code', '@INPUT@', '@OUTPUT@'],
)

wayland_scanner_client = generator(
	wayland_scanner_prog,
	output: '@BASENAME@-client-header.h',
	arguments: ['client-header', '@INPUT@', '@OUTPUT@'],
)

client_protocols = [
	wl_protocol_dir / 'stable/xdg-shell/xdg-shell.xml',
	'wlr-layer-shell-unstable-v1.xml',
	'idle.xml',
	'wlr-foreign-toplevel-management-unstable-v1.xml',
]

foreach p : client_protocols
	wayland_scanner_code.process(p)
	wayland_scanner_client.process(p)
endforeach
