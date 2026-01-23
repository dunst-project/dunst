#!/bin/sh

# Fallback version here
version="1.13.1 (2026-01-23)"

version="$(git describe --tags 2>/dev/null || echo "$version")"

# No leading newline
printf "%s" "$version"
