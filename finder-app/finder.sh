#!/bin/sh
# Finder shell script
# Author: Youssef Salama

set -e
set -u


if [ $# -ne 2 ]
then
	if [ $# -eq 0 ]; then
		echo "Directory path and search string not specified"
	else
		if [ $# -eq 1 ]; then
			echo "Search string not specified"
		else
			echo "Number of parameters specified greater than needed"
		fi
	fi
	exit 1
else
	DIR_PATH=$1
	SEARCH_STR=$2
fi

if [ ! -d $DIR_PATH ]; then
	echo "Directory doesn't exist"
	exit 1
fi

NUM_FILES=$(find $DIR_PATH -type f | wc -l)
NUM_MATCHES=$(grep -r $SEARCH_STR $DIR_PATH | wc -l)

echo "The number of files are $NUM_FILES and the number of matching lines are $NUM_MATCHES"
exit 0