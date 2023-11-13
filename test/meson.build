test_src_files = [
	'dbus.c',
	'draw.c',
	'dunst.c',
	'helpers.c',
	'icon-lookup.c',
	'icon.c',
	'ini.c',
	'input.c',
	'log.c',
	'markup.c',
	'menu.c',
	'misc.c',
	'notification.c',
	'option_parser.c',
	'queues.c',
	'rules.c',
	'setting.c',
	'settings_data.c',
	'test.c',
	'utils.c',
]

foreach dunst_src_file : dunst_src_files
	src_file = dunst_src_file.split('/')[-1]
	if src_file not in test_src_files
		test_src_files += '..' / 'src' / dunst_src_file
	endif
endforeach

test_prog = executable('test-runner',
	test_src_files,
	dependencies: dunst_depends,
	c_args: c_version_arg,
	install: false,
)

test('Run tests', test_prog)
