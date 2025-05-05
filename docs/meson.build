man1 = get_option('mandir') / 'man1'
man5 = get_option('mandir') / 'man5'
pod2man_version_arg = '--release=@0@'.format(meson.project_version())

dunst1 = configure_file(
    input: 'dunst.1.pod.in',
    output: 'dunst.1.pod',
    configuration: conf_data,
)

custom_target(
    'dunst1_pod2man',
    input: dunst1,
    output: 'dunst.1',
    command: [
        pod2man,
        '--name=dunst',
        '--center=Dunst Reference',
        '--section=1',
        pod2man_version_arg,
        '@INPUT@',
        '@OUTPUT@',
    ],
    install: true,
    install_dir: man1,
)

custom_target(
    'dunst5_pod2man',
    input: 'dunst.5.pod',
    output: 'dunst.5',
    command: [
        pod2man,
        '--name=dunst',
        '--center=Dunst Reference',
        '--section=5',
        pod2man_version_arg,
        '@INPUT@',
        '@OUTPUT@',
    ],
    install: true,
    install_dir: man5,
)

custom_target(
    'dunstctl_pod2man',
    input: 'dunstctl.pod',
    output: 'dunstctl.1',
    command: [
        pod2man,
        '--name=dunstctl',
        '--center=Dunstctl Reference',
        '--section=1',
        pod2man_version_arg,
        '@INPUT@',
        '@OUTPUT@',
    ],
    install: true,
    install_dir: man1,
)

custom_target(
    'dunstify_pod2man',
    input: 'dunstify.pod',
    output: 'dunstify.1',
    command: [
        pod2man,
        '--name=dunstify',
        '--center=Dunstify Reference',
        '--section=1',
        pod2man_version_arg,
        '@INPUT@',
        '@OUTPUT@',
    ],
    install: true,
    install_dir: man1,
)
