function __fish_dunstctl_info
    dunstctl (string split ' ' $argv[1]) | jq -r ".data[][] | \"\(.$argv[2].data)\t\(.$argv[3].data)\""
end

function __fish_dunstctl_rule_complete
    set -l parts (string split ' ' $argv[1])
    if test (count $parts[-1]) -eq 0 || test $parts[-2] = rule
        __fish_dunstctl_info 'rules --json' name enabled
        return
    end
    # TODO? enable disable might not make sense when the rule is already in the correct state
    echo -e 'enable\ndisable\ntoggle'
end

# commands
complete -c dunstctl -f -n __fish_use_subcommand -a action -d 'Perform the default action, or open the context menu of the notification at the given position'
complete -c dunstctl -f -n __fish_use_subcommand -a close -d 'Close the last notification or optionally the notification with given ID'
complete -c dunstctl -f -n __fish_use_subcommand -a close-all -d 'Close all the notifications'
complete -c dunstctl -f -n __fish_use_subcommand -a context -d 'Open context menu'
complete -c dunstctl -f -n __fish_use_subcommand -a count -d 'Show the number of notifications'
complete -c dunstctl -f -n __fish_use_subcommand -a history -d 'Display notification history (in JSON)'
complete -c dunstctl -f -n __fish_use_subcommand -a history-clear -d 'Delete all notifications from history'
complete -c dunstctl -f -n __fish_use_subcommand -a history-pop -d 'Pop the latest notification from history or optionally the notification with given ID'
complete -c dunstctl -f -n __fish_use_subcommand -a history-rm -d 'Remove the notification from history with given ID'
complete -c dunstctl -f -n __fish_use_subcommand -a is-paused -d 'Check if pause level is greater than 0'
complete -c dunstctl -f -n __fish_use_subcommand -a set-paused -d 'Set the pause status'
complete -c dunstctl -f -n __fish_use_subcommand -a get-pause-level -d 'Get the current pause level'
complete -c dunstctl -f -n __fish_use_subcommand -a set-pause-level -d 'Set the pause level'
complete -c dunstctl -f -n __fish_use_subcommand -a rules -d 'Displays configured rules (optionally in JSON)'
complete -c dunstctl -f -n __fish_use_subcommand -a rule -d 'Enable or disable a rule by its name'
complete -c dunstctl -f -n __fish_use_subcommand -a reload -d 'Reload the settings of the running instance, optionally with specific configuration files'
complete -c dunstctl -f -n __fish_use_subcommand -a debug -d 'Print debugging information'
complete -c dunstctl -f -n __fish_use_subcommand -a help -d 'Show help'

# command specific arguments
complete -c dunstctl -x -n '__fish_seen_subcommand_from action close close-all context history history-clear is-paused get-pause-level set-pause-level debug help'
complete -c dunstctl -x -n '__fish_seen_subcommand_from count' -a 'displayed history waiting'
complete -c dunstctl -x -n '__fish_seen_subcommand_from history-pop history-rm' -a '(__fish_dunstctl_info history id appname)'
complete -c dunstctl -x -n '__fish_seen_subcommand_from set-paused' -a 'true false toggle'
complete -c dunstctl -x -n '__fish_seen_subcommand_from rule' -a '(__fish_dunstctl_rule_complete (commandline -c))'
complete -c dunstctl -x -n '__fish_seen_subcommand_from rules' -a --json
complete -c dunstctl -n '__fish_seen_subcommand_from reload'

# ex: filetype=fish
