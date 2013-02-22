#!/bin/bash

function keypress {
    echo "press enter to continue..."
    read key
}

function basic_notifications {
    ./notify.py -a "dunst tester" -s "normal" -b "<i>italic body</i>" -u "n"
    ./notify.py -a "dunst tester" -s "critical" -b "<b>bold body</b>" -u "c"
    ./notify.py -a "dunst tester" -s "long body" -b "This is a notification with a very long body"
    ./notify.py -a "dunst tester" -s "duplucate"
    ./notify.py -a "dunst tester" -s "duplucate"
    ./notify.py -a "dunst tester" -s "duplucate"
    ./notify.py -a "dunst tester" -s "url" -b "www.google.de"

}

function show_age {
    killall dunst
    ../dunst -config dunstrc.show_age &
    ./notify.py -a "dunst tester" -s "Show Age" -b "These should print their age after 2 seconds" -u "c"
    basic_notifications
    keypress
}

function run_script {
    killall dunst
    PATH=".:$PATH" ../dunst -config dunstrc.run_script &
    ./notify.py -a "dunst tester" -s "Run Script" -b "After Keypress, 2 other notification should pop up. THis needs notify-send installed" -u "c"
    keypress
    ./notify.py -a "dunst tester" -s "trigger" -b "this should trigger a notification" -u "c"
    keypress
}

function ignore_newline {
    killall dunst
    ../dunst -config dunstrc.ignore_newline_no_wrap &
    ./notify.py -a "dunst tester" -s "Ignore Newline No Wrap" -b "There should be no newline anywhere" -u "c"
    ./notify.py -a "dunst tester" -s "Th\nis\n\n\n is\n fu\nll of \n" -s "\nnew\nlines" -u "c"
    basic_notifications
    keypress

    killall dunst
    ../dunst -config dunstrc.ignore_newline &
    ./notify.py -a "dunst tester" -s "Ignore Newline" -b "The only newlines you should encounter here are wordwraps. That's why I'm so long." -u "c"
    ./notify.py -a "dunst tester" -s "Th\nis\n\n\n is\n fu\nll of \n" -b "\nnew\nlines" -u "c"
    basic_notifications
    keypress
}

function replace {
    killall dunst
    ../dunst -config dunstrc.default &
    id=$(./notify.py -a "dunst tester" -s "Replace" -b "this should get replaces after keypress" -p)
    keypress
    id=$(./notify.py -a "dunst tester" -s "Success?" -b "I hope this is not a new notification" -r $id)
    keypress

}

function markup {
    killall dunst
    ../dunst -config dunstrc.markup "200x0+10+10" &
    ./notify.py -a "dunst tester" -s "Markup Tests" -u "c"
    ./notify.py -a "dunst tester" -s "<b>bold</b> <i>italic</i>"
    ./notify.py -a "dunst tester" -s "<b>broken markup</i>"
    keypress

    killall dunst
    ../dunst -config dunstrc.nomarkup "200x0+10+10" &
    ./notify.py -a "dunst tester" -s "NO Markup Tests" -u "c"
    ./notify.py -a "dunst tester" -s "<b>bold</b><i>italic</i>"
    ./notify.py -a "dunst tester" -s "<b>broken markup</i>"
    keypress

}

function corners {
    killall dunst
    ../dunst -config dunstrc.default -geom "200x0+10+10" &
    ./notify.py -a "dunst tester" -s "upper left" -u "c"
    basic_notifications
    keypress

    killall dunst
    ../dunst -config dunstrc.default -geom "200x0-10+10" &
    ./notify.py -a "dunst tester" -s "upper right" -u "c"
    basic_notifications
    keypress

    killall dunst
    ../dunst -config dunstrc.default -geom "200x0-10-10" &
    ./notify.py -a "dunst tester" -s "lower right" -u "c"
    basic_notifications
    keypress

    killall dunst
    ../dunst -config dunstrc.default -geom "200x0+10-10" &
    ./notify.py -a "dunst tester" -s "lower left" -u "c"
    basic_notifications
    keypress

}

function geometry {
    killall dunst
    ../dunst -config dunstrc.default -geom "0x0" &
    ./notify.py -a "dunst tester" -s "0x0" -u "c"
    basic_notifications
    keypress


    killall dunst
    ../dunst -config dunstrc.default -geom "200x0" &
    ./notify.py -a "dunst tester" -s "200x0" -u "c"
    basic_notifications
    keypress

    killall dunst
    ../dunst -config dunstrc.default -geom "200x2" &
    ./notify.py -a "dunst tester" -s "200x2" -u "c"
    basic_notifications
    keypress

    killall dunst
    ../dunst -config dunstrc.default -geom "200x1" &
    ./notify.py -a "dunst tester" -s "200x1" -u "c"
    basic_notifications
    keypress

    killall dunst
    ../dunst -config dunstrc.default -geom "0x1" &
    ./notify.py -a "dunst tester" -s "0x1" -u "c"
    basic_notifications
    keypress

    killall dunst
    ../dunst -config dunstrc.default -geom "-300x1" &
    ./notify.py -a "dunst tester" -s "-300x1" -u "c"
    basic_notifications
    keypress

    killall dunst
    ../dunst -config dunstrc.default -geom "-300x1-20-20" &
    ./notify.py -a "dunst tester" -s "-300x1-20-20" -u "c"
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
    corners
    geometry
fi

killall dunst
