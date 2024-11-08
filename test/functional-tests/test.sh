#!/bin/bash

# prefix should be the root of the repository
PREFIX="${PREFIX:-../..}"
TESTDIR="$PREFIX/test/functional-tests"
DUNST="${DUNST:-$PREFIX/dunst}"
DUNSTIFY="${DUNSTIFY:-$PREFIX/dunstify}"
DUNSTCTL="${DUSNTCTL:-$PREFIX/dunstctl}"

# for run_script
export PATH="$TESTDIR:$PATH"

function keypress {
    echo "press enter to continue..."
    read key
}

function tmp_dunstrc {
        cp "$TESTDIR/$1" "$TESTDIR/dunstrc.tmp"
        echo -e "\n$2" >> "$TESTDIR/dunstrc.tmp"
}

function tmp_clean {
    rm "$TESTDIR/dunstrc.tmp"
}

function start_dunst {
        killall dunst 2>/dev/null
        $DUNST -config "$TESTDIR/$1" &
        sleep 0.05
}

function basic_notifications {
    $DUNSTIFY -a "dunst tester"         "normal"    "<i>italic body</i>"
    $DUNSTIFY -a "dunst tester"  -u c   "critical"   "<b>bold body</b>"
    $DUNSTIFY -a "dunst tester"         "long body"  "This is a notification with a very long body"
    $DUNSTIFY -a "dunst tester"         "duplicate"
    $DUNSTIFY -a "dunst tester"         "duplicate"
    $DUNSTIFY -a "dunst tester"         "duplicate"
    $DUNSTIFY -a "dunst tester"         "url"        "www.google.de"

}

function show_age {
    echo "###################################"
    echo "show age"
    echo "###################################"
    start_dunst dunstrc.show_age
    $DUNSTIFY -a "dunst tester"  -u c "Show Age" "These should print their age after 2 seconds"
    basic_notifications
    keypress
}

function run_script {
    echo "###################################"
    echo "run script"
    echo "###################################"
    start_dunst dunstrc.run_script
    $DUNSTIFY -a "dunst tester" -u c \
        "Run Script" "After Keypress, 2 other notification should pop up."
    keypress
    $DUNSTIFY -a "dunst tester" -u c "trigger" "this should trigger a notification"
    keypress
}

function replace {
    echo "###################################"
    echo "replace"
    echo "###################################"
    start_dunst dunstrc.default
    id=$($DUNSTIFY -a "dunst tester" -p "Replace" "this should get replaces after keypress")
    keypress
    $DUNSTIFY -a "dunst tester" -r $id "Success?" "I hope this is not a new notification"
    keypress

}

function limit {
    echo "###################################"
    echo "limit"
    echo "###################################"
    tmp_dunstrc dunstrc.default "notification_limit=4"
    start_dunst dunstrc.tmp
    $DUNSTIFY -a "dunst tester" -u c "notification limit = 4"
    basic_notifications
    tmp_clean
    keypress

    tmp_dunstrc dunstrc.default "notification_limit=0"
    start_dunst dunstrc.tmp
    $DUNSTIFY -a "dunst tester" -u c "notification limit = 0 (unlimited notifications)"
    basic_notifications
    tmp_clean
    keypress
}

function markup {
    echo "###################################"
    echo "markup"
    echo "###################################"
    start_dunst dunstrc.default
    $DUNSTIFY -a "dunst tester"  "Markup Tests" -u "c"
    $DUNSTIFY -a "dunst tester"  "Title" "<b>bold</b> <i>italic</i>"
    $DUNSTIFY -a "dunst tester"  "Title" '<a href="github.com"> Github link </a>'
    $DUNSTIFY -a "dunst tester"  "Title" "<b>broken markup</i>"
    keypress

    start_dunst dunstrc.nomarkup
    $DUNSTIFY -a "dunst tester" -u c "No markup Tests" "Titles shoud still be in bold and body in italics"
    $DUNSTIFY -a "dunst tester" "Title" "<b>bold</b><i>italic</i>"
    $DUNSTIFY -a "dunst tester" "Title" "<b>broken markup</i>"
    keypress

}

function test_origin {
    tmp_dunstrc dunstrc.default "origin = $1\n offset = 10x10"
    start_dunst dunstrc.tmp
    $DUNSTIFY -a "dunst tester" -u c "$1"
    basic_notifications
    tmp_clean
    keypress
}

function origin {
    echo "###################################"
    echo "origin"
    echo "###################################"
    test_origin "top-left"
    test_origin "top-center"
    test_origin "top-right"
    test_origin "left-center"
    test_origin "center"
    test_origin "right-center"
    test_origin "bottom-left"
    test_origin "bottom-center"
    test_origin "bottom-right"
}

function test_width {
    tmp_dunstrc dunstrc.default "width = $1"
    start_dunst dunstrc.tmp
    $DUNSTIFY -a "dunst tester" -u c "width = $1"
    basic_notifications
    tmp_clean
    keypress
}

function width {
    echo "###################################"
    echo "width"
    echo "###################################"
    test_width "100"
    test_width "200"
    test_width "400"
    test_width "(0,400)"
}

function test_height {
    tmp_dunstrc dunstrc.default "height = $1"
    start_dunst dunstrc.tmp
    $DUNSTIFY -a "dunst tester" -u c "height = $1"
    $DUNSTIFY -a "dunst tester" -u c "Temporibus accusantium libero sequi at nostrum dolor sequi sed. Cum minus reprehenderit voluptatibus laboriosam et et ut. Laudantium blanditiis omnis ipsa rerum quas velit ut. Quae voluptate soluta enim consequatur libero eum similique ad. Veritatis neque consequatur et aperiam quisquam id nostrum. Consequatur voluptas aut ut omnis atque cum perferendis. Possimus laudantium tempore iste qui nemo voluptate quod. Labore totam debitis consectetur amet. Maxime quibusdam ipsum voluptates quod ex nam sunt. Officiis repellat quod maxime cumque tenetur. Veritatis labore aperiam repellendus. Provident dignissimos ducimus voluptates."
    basic_notifications
    tmp_clean
    keypress
}

function test_progress_bar_alignment {
    tmp_dunstrc dunstrc.default "progress_bar_horizontal_alignment = $1\n progress_bar_max_width = 200"
    start_dunst dunstrc.tmp
    $DUNSTIFY -a "dunst tester" -u c "alignment = $1"
    $DUNSTIFY -h int:value:33 -a "dunst tester" -u n "The progress bar should not be the entire width"
    $DUNSTIFY -h int:value:33 -a "dunst tester" -u c "Short"
    tmp_clean
    keypress
}

function height {
    echo "###################################"
    echo "height"
    echo "###################################"
    test_height "50"
    test_height "100"
    test_height "200"
}

function progress_bar {
    echo "###################################"
    echo "progress_bar"
    echo "###################################"
    start_dunst dunstrc.default
    $DUNSTIFY -h int:value:0 -a "dunst tester" -u c "Progress bar 0%: "
    $DUNSTIFY -h int:value:33 -a "dunst tester" -u c "Progress bar 33%: "
    $DUNSTIFY -h int:value:66 -a "dunst tester" -u c "Progress bar 66%: "
    $DUNSTIFY -h int:value:100 -a "dunst tester" -u c "Progress bar 100%: "
    keypress

    start_dunst dunstrc.default
    $DUNSTIFY -h int:value:33 -a "dunst tester" -u l "Low priority: "
    $DUNSTIFY -h int:value:33 -a "dunst tester" -u n "Normal priority: "
    $DUNSTIFY -h int:value:33 -a "dunst tester" -u c "Critical priority: "
    keypress

    start_dunst dunstrc.progress_bar
    $DUNSTIFY -h int:value:33 -a "dunst tester" -u n "The progress bar should not be the entire width"
    $DUNSTIFY -h int:value:33 -a "dunst tester" -u n "You might also notice height and frame size are changed"
    $DUNSTIFY -h int:value:33 -a "dunst tester" -u c "Short"
    keypress
    test_progress_bar_alignment "left"
    test_progress_bar_alignment "center"
    test_progress_bar_alignment "right"
}

function icon_position {
    echo "###################################"
    echo "icon_position"
    echo "###################################"
    padding_cases=(
        '0 0 0 no padding'
        '15 1 1 vertical'
        '1 50 1 horizontal'
        '1 1 25 icon '
    )

    for padding_case in "${padding_cases[@]}"; do
        read vertical horizontal icon label <<<"$padding_case"

        padding_settings="
            padding = $vertical
            horizontal_padding = $horizontal
            text_icon_padding = $icon
        "

        tmp_dunstrc dunstrc.icon_position "$padding_settings"
        start_dunst dunstrc.tmp

        for position in left top right off; do
            for alignment in left center right; do
                category="icon-$position-alignment-$alignment"
                $DUNSTIFY -a "dunst tester" --hints string:category:$category -u n "$category"$'\n'"padding emphasis: $label"
            done
        done
        tmp_clean
        keypress
    done
}

function hide_text {
    echo "###################################"
    echo "hide_text"
    echo "###################################"
    start_dunst dunstrc.hide_text
    $DUNSTIFY -a "dunst tester" -u c "text not hidden" "You should be able to read me!\nThe next notifications should not have any text."
    local hidden_body="If you can read me then hide_text is not working."
    $DUNSTIFY -a "dunst tester" -u l "text hidden" "$hidden_body"
    $DUNSTIFY -a "dunst tester" -h int:value:$((RANDOM%100)) -u l "text hidden + progress bar" "$hidden_body"
    $DUNSTIFY -a "dunst tester" -u n "text hidden + icon" "$hidden_body"
    $DUNSTIFY -a "dunst tester" -h int:value:$((RANDOM%100)) -u n "text hidden + icon + progress bar" "$hidden_body"
    keypress
}

function gaps {
    echo "###################################"
    echo "gaps"
    echo "###################################"
    start_dunst dunstrc.gaps
    CHOICE=$($DUNSTIFY -a "dunst tester" -A "default,Default" -A "optional,Optional" "Click #1" -u l) \
        && echo Clicked $CHOICE for \#1 &
    CHOICE=$($DUNSTIFY -a "dunst tester" -A "default,Default" -A "optional,Optional" "Click #2" -u n) \
        && echo Clicked $CHOICE for \#2 &
    CHOICE=$($DUNSTIFY -a "dunst tester" -A "default,Default" -A "optional,Optional" "Click #3" -u c) \
        && echo Clicked $CHOICE for \#3 &
    CHOICE=$($DUNSTIFY -a "dunst tester" -A "default,Default" -A "optional,Optional" "Click #4" -u l) \
        && echo Clicked $CHOICE for \#4 &
    CHOICE=$($DUNSTIFY -a "dunst tester" -A "default,Default" -A "optional,Optional" "Click #5" -u n) \
        && echo Clicked $CHOICE for \#5 &
    CHOICE=$($DUNSTIFY -a "dunst tester" -A "default,Default" -A "optional,Optional" "Click #6" -u c) \
        && echo Clicked $CHOICE for \#6 &
    keypress
}

function separator_click {
    echo "###################################"
    echo "separator_click"
    echo "###################################"
    start_dunst dunstrc.separator_click
    CHOICE=$($DUNSTIFY -a "dunst tester" -A "default,Default" -A "optional,Optional" "Click #1" -u l) \
        && echo Clicked $CHOICE for \#1 &
    CHOICE=$($DUNSTIFY -a "dunst tester" -A "default,Default" -A "optional,Optional" "Click #2" -u c) \
        && echo Clicked $CHOICE for \#2 &
    CHOICE=$($DUNSTIFY -a "dunst tester" -A "default,Default" -A "optional,Optional" "Click #3" -u n) \
        && echo Clicked $CHOICE for \#3 &
    keypress
}

function dynamic_height {
    echo "###################################"
    echo "dynamic_height"
    echo "###################################"

    for max in 50 100 200 ""; do
        for min in 50 100 200 ""; do
            [[ $min -gt $max ]] && continue

            tmp_dunstrc dunstrc.vertical_align "height = ($min, $max)"
            start_dunst dunstrc.tmp

            $DUNSTIFY -a "dunst tester" -u l "text" "height min = $min, max = $max"
            $DUNSTIFY -a "dunst tester" -h int:value:$((RANDOM%100)) -u l "text + progress bar" "height min = $min, max = $max"
            $DUNSTIFY -a "dunst tester" -u n "text + icon" "height min = $min, max = $max"
            $DUNSTIFY -a "dunst tester" -h int:value:$((RANDOM%100)) -u n "text + icon + progress bar" "height min = $min, max = $max"

            $DUNSTIFY -a "dunst tester" -h string:category:hide -u l "text hidden" "SHOULD BE NOT VISIBLE"
            $DUNSTIFY -a "dunst tester" -h string:category:hide -h int:value:$((RANDOM%100)) -u l "text hidden + progress bar" "SHOULD BE NOT VISIBLE"
            $DUNSTIFY -a "dunst tester" -h string:category:hide -u n "text hidden + icon" "SHOULD BE NOT VISIBLE"
            $DUNSTIFY -a "dunst tester" -h string:category:hide -h int:value:$((RANDOM%100)) -u n "text hidden + icon + progress bar" "SHOULD BE NOT VISIBLE"

            tmp_clean
            keypress
        done
    done
}

function vertical_align {
    echo "###################################"
    echo "vertical_align"
    echo "###################################"

    padding_cases=(
        '0 0 0 0 none'
        '1 1 1 50 less'
        '1 1 1 100 slight'
        '1 1 1 200 more'
    )

    for valign in top center bottom; do
        for padding_case in "${padding_cases[@]}"; do
            read vertical horizontal icon height label <<<"$padding_case"

            padding_settings="
                padding = $vertical
                horizontal_padding = $horizontal
                text_icon_padding = $icon
                vertical_alignment = $valign
                height = ($height, )
            "

            tmp_dunstrc dunstrc.vertical_align "$padding_settings"
            start_dunst dunstrc.tmp

            for position in left top right off; do
                for alignment in left center right; do
                    category="icon-$position-alignment-$alignment"
                    $DUNSTIFY -a "dunst tester" --hints string:category:$category -u n "$category"$'\n'"emphasis: $label"$'\n'"vertical alignment: $valign"
                done
            done

            keypress
            start_dunst dunstrc.tmp

            for position in left top right; do
                for alignment in left center right; do
                    category="icon-$position-alignment-$alignment-hide"
                    $DUNSTIFY -a "dunst tester" --hints string:category:$category -u n "$category"$'\n'"emphasis: $label"$'\n'"vertical alignment: $valign"
                done
            done

            tmp_clean
            keypress
        done
    done
}

function hot_reload {
    echo "###################################"
    echo "hot_reload"
    echo "###################################"

    tmp_dunstrc dunstrc.default "background = \"#313131\""
    start_dunst dunstrc.tmp

    $DUNSTIFY -a "dunst tester" "Nice notification" "This will change once"
    $DUNSTIFY -a "dunst tester" --hints string:category:change "Change" "And this too"
    keypress

    tmp_dunstrc dunstrc.default "
        [change]
        category = change
        background = \"#525\"
        set_category = notchange
        urgency = critical
    "
    $DUNSTCTL reload
    keypress

    $DUNSTCTL reload
    keypress

    $DUNSTCTL reload "$TESTDIR/dunstrc.hot_reload"
    $DUNSTIFY -a "dunst tester" "New notification" "Now we are using another config file"
    keypress

    tmp_clean
}

function dmenu_order {
    echo "###################################"
    echo "dmenu_order"
    echo "###################################"

    tmp_dunstrc dunstrc.default "sort=urgency"
    start_dunst dunstrc.tmp

    for i in critical normal low
    do
        $DUNSTIFY -a "dunst tester" -u $i --action="replyAction,Reply" --action="forwardAction,Forward" "Message Received $i" "Sorted by urgency" &
        sleep .5
    done

    $DUNSTCTL context
    keypress

    tmp_dunstrc dunstrc.default "sort=update"
    start_dunst dunstrc.tmp

    for i in critical normal low
    do
        $DUNSTIFY -a "dunst tester" -u $i --action="replyAction,Reply" --action="forwardAction,Forward" "Message Received $i" "Sorted by time" &
        sleep .5
    done

    $DUNSTCTL context
    keypress

    tmp_dunstrc dunstrc.default "sort=false"
    start_dunst dunstrc.tmp

    for i in 69 98 34 1
    do
        $DUNSTIFY -a "dunst tester" -r $i --action="replyAction,Reply" --action="forwardAction,Forward" "Message Received $i" "Sorted by id" &
        sleep .5
    done

    $DUNSTCTL context
    keypress
    tmp_clean
}

if [ -n "$1" ]; then
    while [ -n "$1" ]; do
        $1
        shift
    done
else
    width
    height
    origin
    limit
    show_age
    run_script
    ignore_newline
    replace
    markup
    progress_bar
    icon_position
    hide_text
    gaps
    separator_click
    dynamic_height
    vertical_align
    hot_reload
    dmenu_order
fi

killall dunst
