#!/bin/sh
tests() {
    #check if these get displayed right
    notify-send "Dinner is ready"
    notify-send "The\nTrick"
    notify-send "The Trick
#2"
}

history_json="$(dunstctl history)"
history_items="$(printf '%s' "$history_json" | jq -r '.data[0][] | .appname.data , (.timestamp.data | tostring) , .summary.data | gsub("[\\n]"; "\\n")')" # the gsub is to really ensure no escaped new lines in the data lead us to print new lines. New lines in data have to be escaped. (Because) Actual newlines are the field separator essential to the logic of the while loop below, and rofi further down.

#history_items ends up looking like arrays with an order with this meaning: 
#appname (newline) timestamp (newline) summary (newline)
#
#NetworkManager
#223948180
#Wi-Fi Networks Available
#
#How do we know where this notification ends and another begins? By length, which is always 3 values.

#history_length="$(($(echo "$history_items" | wc -l) / 3 ))"

iter=0

IFS='
'
while read -r application_name; read -r notification_timestamp ; read -r notification_summary
do
    iter=$((iter+1))
    system_timestamp=$(cat /proc/uptime|cut -d'.' -f1)
    how_long_ago=$((system_timestamp - notification_timestamp / 1000000))
    notification_time=$(date +%X -d "$(date) - $how_long_ago seconds")

    option=$(printf '"%s" (%s) - %s - %04d' "$notification_summary" "$application_name" "$notification_time" "$iter")

    options="$options$option
"
done <<EOF
$history_items
EOF

options="$options""Cancel"

result=$(printf '%s' "$options" | rofi -dmenu -i)
if [ "$?" -ne 0 ] || [ -z "$result" ] || [ "$result" = "Cancel" ]; then
    exit
fi

# Get the internal notification ID
selection_index=$(( $(printf '%s' "$result" | awk -F" " '{print $(NF-0)}') - 1)) # translation of awk: grab the last field
notification_id=$(printf '%s' "$history_json" | jq -r .data[][$selection_index].id.data)

# Tell dunst to revive said notification
dunstctl history-pop $notification_id
