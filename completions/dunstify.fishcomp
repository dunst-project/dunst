if command -q jq
    function __fish_dunstify_history
        dunstctl history | jq -r '.data[][] | "\(.id.data)\t\(.appname.data)"'
    end
else
    function __fish_dunstify_history
        dunstctl history | awk '/"id" :/ {getline; getline; print $3}'
    end
end

complete -c dunstify -s '?' -l help -d 'Show help options'
complete -c dunstify -s a -l app-name -r -d 'Name of your application'
complete -c dunstify -s u -l urgency -x -a 'low normal critical' -d 'The urgency of this notification'
complete -c dunstify -s h -l hint -x -d 'User specified hints'
complete -c dunstify -s A -l action -x -d 'Actions the user can invoke'
complete -c dunstify -s t -l expire-time -x -d 'The time in milliseconds until the notification expires'
complete -c dunstify -s i -l icon -x -d 'An Icon that should be displayed with the notification'
complete -c dunstify -s I -l raw-icon -r -d 'Path to the icon to be sent as raw image data'
complete -c dunstify -s c -l category -d 'The category of this notification'
complete -c dunstify -l capabilities -d 'Print the server capabilities and exit'
complete -c dunstify -l serverinfo -d 'Print server information and exit'
complete -c dunstify -s p -l print-id -d 'Print id, which can be used to update/replace this notification'
complete -c dunstify -s r -l replace-id -x -a '(__fish_dunstify_history)' -d 'Set id of this notification.'
complete -c dunstify -s C -l close -x -a '(__fish_dunstify_history)' -d 'Close the notification with the specified ID'
complete -c dunstify -s w -l wait -d 'Block until notification is closed and print close reason'
complete -c dunstify -s e -l transient -d 'Mark the notification as transient'
complete -c dunstify -l stack-tag -d 'Set dunst stack tag hint'
complete -c dunstify -s v -l version -d 'Print version information'

# ex: filetype=fish
