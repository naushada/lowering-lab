#!/bin/sh
# Rough instruction count: indented lines starting with a mnemonic.
n=`grep -cE '^[[:space:]]+[a-z]' "$1" 2>/dev/null` || n=0
echo "$n"
