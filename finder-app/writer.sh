#!/bin/sh
# Writer shell script
# Author: Youssef Salama

set -u


if [ $# -ne 2 ]
then
	if [ $# -eq 0 ]; then
		echo "Write file path and write string not specified"
	else
		if [ $# -eq 1 ]; then
			echo "Write string not specified"
		else
			echo "Number of parameters specified greater than needed"
		fi
	fi
	exit 1
else
	WRITE_FILE_PATH=$1
	WRITE_STR=$2
fi


if [ ! -e $WRITE_FILE_PATH ]; then
	mkdir -p $(dirname "$WRITE_FILE_PATH") && touch $WRITE_FILE_PATH 2> /dev/null
	if [ $? -ne 0 ]; then
		echo "Can't create file $WRITE_FILE_PATH"
		exit 1
	fi
fi

echo $WRITE_STR > $WRITE_FILE_PATH
exit 0