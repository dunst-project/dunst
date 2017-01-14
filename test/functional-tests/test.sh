#!/bin/bash

function keypress {
    echo "press enter to continue..."
    read key
}

function basic_notifications {
    ../../dunstify -a "dunst tester"         "normal"    "<i>italic body</i>"
    ../../dunstify -a "dunst tester"  -u c   "critical"   "<b>bold body</b>"
    ../../dunstify -a "dunst tester"         "long body"  "This is a notification with a very long body"
    ../../dunstify -a "dunst tester"         "duplucate"
    ../../dunstify -a "dunst tester"         "duplucate"
    ../../dunstify -a "dunst tester"         "duplucate"
    ../../dunstify -a "dunst tester"         "url"        "www.google.de"

}

function show_age {
    echo "###################################"
    echo "show age"
    echo "###################################"
    killall dunst
    ../../dunst -config dunstrc.show_age &
    ../../dunstify -a "dunst tester"  -u c "Show Age" "These should print their age after 2 seconds"
    basic_notifications
    keypress
}

function run_script {
    echo "###################################"
    echo "run script"
    echo "###################################"
    killall dunst
    PATH=".:$PATH" ../../dunst -config dunstrc.run_script &
    ../../dunstify -a "dunst tester" -u c \
        "Run Script" "After Keypress, 2 other notification should pop up. THis needs notify-send installed"
    keypress
    ../../dunstify -a "dunst tester" -u c "trigger" "this should trigger a notification"
    keypress
}

function ignore_newline {
    echo "###################################"
    echo "ignore newline"
    echo "###################################"
    killall dunst
    ../../dunst -config dunstrc.ignore_newline_no_wrap &
    ../../dunstify -a "dunst tester" -u c "Ignore Newline No Wrap" "There should be no newline anywhere"
    ../../dunstify -a "dunst tester" -u c "Th\nis\n\n\n is\n fu\nll of \n" "\nnew\nlines"
    basic_notifications
    keypress

    killall dunst
    ../../dunst -config dunstrc.ignore_newline &
    ../../dunstify -a "dunst tester" -u c "Ignore Newline" \
        "The only newlines you should encounter here are wordwraps. That's why I'm so long."
    ../../dunstify -a "dunst tester" -u c "Th\nis\n\n\n is\n fu\nll of \n" "\nnew\nlines"
    basic_notifications
    keypress
}

function replace {
    echo "###################################"
    echo "replace"
    echo "###################################"
    killall dunst
    ../../dunst -config dunstrc.default &
    id=$(../../dunstify -a "dunst tester" -p "Replace" "this should get replaces after keypress")
    keypress
    ../../dunstify -a "dunst tester" -r $id "Success?" "I hope this is not a new notification"
    keypress

}

function markup {
    echo "###################################"
    echo "markup"
    echo "###################################"
    killall dunst
    ../../dunst -config dunstrc.markup "200x0+10+10" &
    ../../dunstify -a "dunst tester"  "Markup Tests" -u "c"
    ../../dunstify -a "dunst tester"  "<b>bold</b> <i>italic</i>"
    ../../dunstify -a "dunst tester"  "<b>broken markup</i>"
    keypress

    killall dunst
    ../../dunst -config dunstrc.nomarkup "200x0+10+10" &
    ../../dunstify -a "dunst tester" -u c "NO Markup Tests"
    ../../dunstify -a "dunst tester" "<b>bold</b><i>italic</i>"
    ../../dunstify -a "dunst tester" "<b>broken markup</i>"
    keypress

}

function corners {
    echo "###################################"
    echo "corners"
    echo "###################################"
    killall dunst
    ../../dunst -config dunstrc.default -geom "200x0+10+10" &
    ../../dunstify -a "dunst tester" -u c "upper left"
    basic_notifications
    keypress

    killall dunst
    ../../dunst -config dunstrc.default -geom "200x0-10+10" &
    ../../dunstify -a "dunst tester" -u c "upper right"
    basic_notifications
    keypress

    killall dunst
    ../../dunst -config dunstrc.default -geom "200x0-10-10" &
    ../../dunstify -a "dunst tester" -u c "lower right"
    basic_notifications
    keypress

    killall dunst
    ../../dunst -config dunstrc.default -geom "200x0+10-10" &
    ../../dunstify -a "dunst tester" -u c "lower left"
    basic_notifications
    keypress

}

function geometry {
    echo "###################################"
    echo "geometry"
    echo "###################################"
    killall dunst
    ../../dunst -config dunstrc.default -geom "0x0" &
    ../../dunstify -a "dunst tester" -u c "0x0"
    basic_notifications
    keypress


    killall dunst
    ../../dunst -config dunstrc.default -geom "200x0" &
    ../../dunstify -a "dunst tester" -u c "200x0"
    basic_notifications
    keypress

    killall dunst
    ../../dunst -config dunstrc.default -geom "200x2" &
    ../../dunstify -a "dunst tester" -u c "200x2"
    basic_notifications
    keypress

    killall dunst
    ../../dunst -config dunstrc.default -geom "200x1" &
    ../../dunstify -a "dunst tester" -u c "200x1"
    basic_notifications
    keypress

    killall dunst
    ../../dunst -config dunstrc.default -geom "0x1" &
    ../../dunstify -a "dunst tester" -u c "0x1"
    basic_notifications
    keypress

    killall dunst
    ../../dunst -config dunstrc.default -geom "-300x1" &
    ../../dunstify -a "dunst tester" -u c "-300x1"
    basic_notifications
    keypress

    killall dunst
    ../../dunst -config dunstrc.default -geom "-300x1-20-20" &
    ../../dunstify -a "dunst tester" -u c "-300x1-20-20"
    basic_notifications
    keypress

    killall dunst
    ../../dunst -config dunstrc.default -geom "x1" &
    ../../dunstify -a "dunst tester" -u c "x1-20-20" "across the screen"
    basic_notifications
    keypress
}

if [ -n "$1" ]; then
    while [ -n "$1" ]; do
        $1
        shift
    done
else
    geometry
    corners
    show_age
    run_script
    ignore_newline
    replace
    markup
fi

killall dunst
