#!/usr/bin/awk -f
# Copyright (c) 2016 Scott Vokes <vokes.s@gmail.com>
#
# Permission to use, copy, modify, and/or distribute this software for any
# purpose with or without fee is hereby granted, provided that the above
# copyright notice and this permission notice appear in all copies.
#
# THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
# WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
# ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
# WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
# ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
# OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

BEGIN {
    GREEN = "\033[32m"
    RED = "\033[31m"
    YELLOW = "\033[33m"
    RESET = "\033[m"
}

/^PASS/ { sub("PASS", GREEN "PASS" RESET) }
/^SKIP/ { sub("SKIP", YELLOW "SKIP" RESET) }
/^FAIL/ { sub("FAIL", RED "FAIL" RESET) }

# highlight hexdump difference markers
/^[0-9a-f]/ {
    sub("X", GREEN "X" RESET, $2)
    gsub("<", GREEN "<" RESET, $0)
}

{ print($0) }
