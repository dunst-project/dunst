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
complete -c dunstify -s a -l appname -r -d 'Name of your application'
complete -c dunstify -s u -l urgency -x -a 'low normal critical' -d 'The urgency of this notification'
complete -c dunstify -s h -l hints -x -d 'User specified hints'
complete -c dunstify -s A -l action -x -d 'Actions the user can invoke'
complete -c dunstify -s t -l timeout -x -d 'The time in milliseconds until the notification expires'
complete -c dunstify -s i -l icon -x -d 'An Icon that should be displayed with the notification'
complete -c dunstify -s I -l raw_icon -r -d 'Path to the icon to be sent as raw image data'
complete -c dunstify -s c -l capabilities -d 'Print the server capabilities and exit'
complete -c dunstify -s s -l serverinfo -d 'Print server information and exit'
complete -c dunstify -s p -l printid -d 'Print id, which can be used to update/replace this notification'
complete -c dunstify -s r -l replace -x -a '(__fish_dunstify_history)' -d 'Set id of this notification.'
complete -c dunstify -s C -l close -x -a '(__fish_dunstify_history)' -d 'Close the notification with the specified ID'
complete -c dunstify -s b -l block -d 'Block until notification is closed and print close reason'

# ex: filetype=fish
