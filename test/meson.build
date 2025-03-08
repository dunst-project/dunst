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

fs = import('fs')
foreach dunst_src_file : dunst_src_files
    if fs.name(dunst_src_file) not in test_src_files
        test_src_files += dunst_src_file
    endif
endforeach

test_prog = executable(
    'test-runner',
    test_src_files,
    dependencies: dunst_depends,
    c_args: c_version_arg,
    install: false,
    build_by_default: false,
)

test(
    'Run tests',
    test_prog,
    env: environment({'TESTDIR': meson.current_source_dir()}),
)
