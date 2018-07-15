#!/usr/bin/env bash
###############################################################################
##
## Usage
##
##     ./<script_name> [<OPTIONS>]
##
##     If it does not run, give execute permissions to the script with
##     chmod +x <script_name>. Then run ./<script_name>.
##
## Options
##
##     -h|--help            Optional. Show help message.
##
## Description
##
##     This script creates a dunst themed user config in $HOME/.config/dunst/
##     folder, changing dunst basic theming options (like fonts, colors, etc.)
##     according to your current X resources color palette.
##
##     To make this possible, it reads your current user config
##     ($HOME/.conf/dunst/dunstrc, which is copied from the default config if
##     it does not exist) and changes the attributes values with  new ones (see
##     Theming section for more info). Then it dumps the new configuration to
##     $user_xr_color_conf file.
##
## Theming
##
##     To change colors and fonts:
##
##     * Firstly you have to ensure that those dunst attributes are defined in
##       the corresponding sections. For example, to change the frame_color
##       value from the global section, in $HOME/.config/dunst/dunstrc should
##       be:
##
##       ...
##       [global]
##           ...
##           frame_color = <value>
##           ...
##
##     * Then, you can change attribute values as you wish with the
##       following format in theme_attr_dict:
##
##       ["<sec>-<attr>"]="<val>|$(xrdb_get '<X_res>' '<dev_val>')"
##
##       Each <variable> means the following:
##
##       * sec - current section name e.g.: global.
##       * attr - current attribute name e.g.: frame-color.
##       * val - a simple value e.g.: "#fffeee", Monospace, 11...
##       * X_res - X resource name e.g.: color8.
##       * dev_val - default value to set if X_res is not found e.g.: #65737e.
##
##       Theming example (you can check theme_attr_dict for other values):
##
##       ["global-frame_color"]="\"$(xrdb_get 'color8' '#65737e')
##
##     The function xrdb_get, searches the first parameter in the X resources
##     database with appres (command installed from xorg-appres in archlinux
##     and x11-utils in ubuntu). If the first parameter does not exist, the
##     function returns the second parameter. For hex colors, is important to
##     scape " characters for proper functioning of dunst config reader (for
##     example "#ffeegg").
##
##     You can define X_res variables in $HOME/.Xresources file. For in depth
##     syntax go to https://wiki.archlinux.org/index.php/X_resources.
##
###############################################################################

set -e

# Check if appres is installed
if [ ! "$(command -v appres)" ]; then
    printf 'Install xorg-appres in archlinux and x11-utils in debian/ubuntu.\n'
    exit 1
fi

readonly script_name="$(basename "$0")"
readonly base_dir="$(realpath "$(dirname "$0")")"

# Show ussage
usage() {
    grep -e '^##[^#]*$' "$base_dir/$script_name"  | \
        sed -e "s/^##[[:space:]]\{0,1\}//g" \
            -e "s/<script_name>/${script_name}/g"
    exit 2
} 2>/dev/null

# Show help
if [ "$#" -gt 0 ]; then
    if ! [[ "$1" == "--help" || "$1" == "-h" ]]; then
        printf '\nUnknown option.\n'
    fi
    usage
fi

# Function to get resource values
xrdb_get () {
    output="$(appres Dunst | grep "$1:" | head -n1 | cut -f2)"
    default="$2"
    printf '%s' "${output:-$default}"
}

#
# Attributes dictionary. Add or remove attributes (see header for more info)
#
declare -A theme_attr_dict=(
    ["global-font"]="$(xrdb_get 'font' 'Monospace') $(xrdb_get '*.font_size:' '11')"
    ["global-frame_width"]="$(xrdb_get 'border_width' '1')"
    ["global-frame_color"]="\"$(xrdb_get 'color8' '#65737e')\""

    ["urgency_low-background"]="\"$(xrdb_get 'color0' '#2b303b')\""
    ["urgency_low-foreground"]="\"$(xrdb_get 'color4' '#65737e')\""
    ["urgency_low-frame_color"]="\"$(xrdb_get 'color4' '#65737e')\""

    ["urgency_normal-background"]="\"$(xrdb_get 'color0' '#2b303b')\""
    ["urgency_normal-foreground"]="\"$(xrdb_get 'color2' '#a3be8c')\""
    ["urgency_normal-frame_color"]="\"$(xrdb_get 'color2' '#a3be8c')\""

    ["urgency_critical-background"]="\"$(xrdb_get 'color0' '#2b303b')\""
    ["urgency_critical-foreground"]="\"$(xrdb_get 'color1' '#bf616a')\""
    ["urgency_critical-frame_color"]="\"$(xrdb_get 'color1' '#bf616a')\""
)

# Attributes dictionary keys.
readonly valid_keys="${!theme_attr_dict[@]}"

#
# File paths
#
# User config dir and file
readonly user_conf_dir="${XDG_CONFIG_HOME:-$HOME/.config}/dunst"
readonly user_conf="$user_conf_dir/dunstrc"

# Default config dir and example file
example_conf_dir="/usr/share/dunst"
example_conf="$example_conf_dir/dunstrc"

# User xresources color config file
readonly user_xr_color_conf="$user_conf_dir/dunstrc_xr_colors"


# Check if the user config directory exists
if ! [ -d "$user_conf_dir" ]; then
    printf 'Creating folder "%s".\n' "$user_conf_dir"
    mkdir -p  "$user_conf_dir"
fi

# Check if the user config file exists
if ! [ -f "$user_conf" ]; then
    printf '"%s" file does not exist.\nChecking for config file example.' \
               "$user_conf"

    if [ -d "/usr/share/dunst" ]; then
        # Archlinux default dir and example file
        example_conf_dir="/usr/share/dunst"

        if [ -f "$example_conf_dir/dunstrc" ]; then
            example_conf="$example_conf_dir/dunstrc"
        else
            printf 'Could not find the example config file in "%s".
               \nPlease, change $example_conf variable in the script.' \
                   "$example_conf_dir"
            exit 1
        fi

    elif [ -d "/usr/share/doc/dunst" ]; then
        # Debian/Ubuntu default dir
        example_conf_dir="/usr/share/doc/dunst"

        if [ -f "$example_conf_dir/dunstrc.example.gz" ]; then
            # Ubuntu <= 17.10 and Debian <= 1.2.0-2 example file:
            example_conf="$example_conf_dir/dunstrc.example.gz"
        elif [ -f "$example_conf_dir/dunstrc.gz" ]; then
            # Ubuntu >= 18.04 and Debian >= 1.3.0-2 example file:
            example_conf="$example_conf_dir/dunstrc.gz"
        else
            printf 'Could not find the example config file in "%s".
               \nPlease, change $example_conf variable in the script.' \
                   "$example_conf_dir"
            exit 1
        fi

    else
        printf 'Could not find the example config directory.
           \nPlease, change $example_conf_dir variable in the script.'
        exit 1
    fi

    printf 'Copying example config to "%s".\n' "$user_conf_dir"

    # Get the extension to check if the file is compressed
    if [[ "${example_conf##*\.}" == "gz" ]]; then
        # Extract example file to user config file
        gunzip -c "$example_conf" > "$user_conf"
    else
        cp "$example_conf" "$user_conf_dir"
    fi

fi

# Regular expressions
readonly re_section_line='^\[(.*)\]$'
readonly re_empty_comment_line='(^$)|(^[[:space:]]*(\#)|(;))'
readonly re_attribute_line='^([[:space:]]*)([_[:alnum:]]+)'

# Create an array with the file lines
mapfile -t conf < "$user_conf"

# Iterate over the file lines
for idx in "${!conf[@]}"; do
    # Current line
    curr_line="${conf[$idx]}"
    # If we are in a new section:
    if [[ "$curr_line" =~ $re_section_line ]]; then
        curr_section="${BASH_REMATCH[1]}"
        continue
    fi
    # Skip the line if it is empty or has a comment
    if [[ "$curr_line" =~ $re_empty_comment_line ]]; then
        continue
    fi
    # Get the attribute in the current line
    [[ "$curr_line" =~ $re_attribute_line ]]
    curr_attr_name="${BASH_REMATCH[2]}"
    curr_sett_name="${curr_section}-${curr_attr_name}"
    # If the current attribute is not in our dictionary, continue
    case "$valid_keys" in
        *"$curr_sett_name"*)
            printf -v conf[$idx] '    %s = %s' \
                   "${curr_attr_name}" \
                   "${theme_attr_dict[$curr_sett_name]}"
            ;;
    esac
done

# Create a header for the xr_color config file
user_xr_color_conf_content="\
###################################
#
# Config file created with
# $script_name wrapper
#
###################################

"

# After everything is completed, write the new config to a file
user_xr_color_conf_content+="$(printf '%s\n' "${conf[@]}")"

printf '%s\n' "$user_xr_color_conf_content" > "$user_xr_color_conf"

printf '"%s" updated successfully.\n' "$user_xr_color_conf"

