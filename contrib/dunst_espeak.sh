#!/bin/bash

summary="$2"
body="$3"

echo "$summary $body" | espeak
