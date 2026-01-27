#!/bin/sh

# Fallback version here
version="1.13.2-non-git"

version="$(git describe --tags 2>/dev/null || echo "$version")"

# No leading newline
printf "%s" "$version"
