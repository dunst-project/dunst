bashdir = get_option('datadir') / 'bash-completion/completions'
fishdir = get_option('datadir') / 'fish/vendor_completions.d'
zshdir = get_option('datadir') / 'zsh/site-functions'

bashcomp = files(
  'dunst.bashcomp',
  'dunstify.bashcomp',
  'dunstctl.bashcomp',
)

bashrename = [
  'dunst',
  'dunstify',
  'dunstctl',
]

install_data(
  bashcomp,
  install_dir: bashdir,
  rename: bashrename,
)

zshcomp = files(
  '_dunst.zshcomp',
  '_dunstify.zshcomp',
  '_dunstctl.zshcomp',
)

zshrename = [
  '_dunst',
  '_dunstify',
  '_dunstctl',
]

install_data(
  zshcomp,
  install_dir: zshdir,
  rename: zshrename,
)

fishcomp = files(
  'dunst.fishcomp',
  'dunstify.fishcomp',
  'dunstctl.fishcomp',
)

fishrename = [
  'dunst.fish',
  'dunstify.fish',
  'dunstctl.fish',
]

install_data(
  fishcomp,
  install_dir: fishdir,
  rename: fishrename,
)
