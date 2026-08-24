#!/bin/bash

# Source file list. Names are NUL separated, so any file name is handled.
readarray -d '' FILE_LIST < <(find . \
	\( -name '*.cpp' -o -name '*.hpp' \) -print0)

if [ ${#FILE_LIST[@]} -eq 0 ]
then
	echo "No source files."
	exit 1
fi

# Trailing whitespace check.
WHITESPACE_CHECK=`grep -HnP "[ \t]+$" "${FILE_LIST[@]}"`

if [ -n "$WHITESPACE_CHECK" ]
then
	echo "Trailing whitespaces in code."
	echo "$WHITESPACE_CHECK"
	exit 1
fi

# Line length check.
LENGTH_CHECK=$(awk '
{
	COLUMNS = 0

	for (I = 1; I <= length($0); I++) {
		if (substr($0, I, 1) == "\t") {
			COLUMNS += 8 - COLUMNS % 8
		} else {
			COLUMNS++
		}
	}

	if (COLUMNS > 80) {
		printf "%s:%d: %d columns\n", FILENAME, FNR, COLUMNS
	}
}' "${FILE_LIST[@]}")

if [ -n "$LENGTH_CHECK" ]
then
	echo "Lines longer than 80 columns."
	echo "$LENGTH_CHECK"
	exit 1
fi

exit 0
