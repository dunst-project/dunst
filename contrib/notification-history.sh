#!/bin/bash

history_json=$(dunstctl history)

history_length=$(echo $history_json|jq -r '.data[] | length')

options=""

for iter in $(seq $history_length); do
    i=$((iter-1))
    application_name=$(echo $history_json|jq -r .data[][$i].appname.data)
    notification_summary=$(echo $history_json|jq -r .data[][$i].summary.data)
    notification_timestamp=$(echo $history_json|jq -r .data[][$i].timestamp.data)
    system_timestamp=$(cat /proc/uptime|cut -d'.' -f1)

    how_long_ago=$((system_timestamp-notification_timestamp/1000000))

    notification_time=$(date +%X -d "$(date) - $how_long_ago seconds")

    option=$(printf '%04d - %s: "%s" (at %s)' "$iter" "$application_name" \
        "$notification_summary" "$notification_time")

    options="$options$option\n"
done
options="$options""Cancel"

result=$(echo -e $options|rofi -dmenu -i)
if [ "$result" = "Cancel" ]; then
    # Exit if cancelled
    exit 0
fi
if [ "$result" = "" ]; then
    # Exit on empty strings
    exit 0
fi
if [ "$?" -ne 0 ]; then
    # Exit on non-zero return values
    exit 0
fi

# Get the internal notification ID
selection_index=$((${result:0:4}-1))
notification_id=$(echo $history_json|jq -r .data[][$selection_index].id.data)

# Tell dunst to revive said notification
dunstctl history-pop $notification_id

