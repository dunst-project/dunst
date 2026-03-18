#!/bin/sh

# Fallback version here
version="1.13.2 (2026-03-18)"

version="$(git describe --tags 2>/dev/null || echo "$version")"

# No leading newline
printf "%s" "$version"
