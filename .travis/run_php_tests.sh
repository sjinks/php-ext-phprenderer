#! /bin/bash

DIR=$(readlink -enq "$(dirname $0)/../")

shopt -s nullglob
export NO_INTERACTION=1
export REPORT_EXIT_STATUS=1
make -C "$DIR" test
for i in $DIR/tests/*.log $DIR/tests/*.mem; do
	echo "====== $i ======";
	cat "$i";
done
[ -n "$(echo $DIR/tests/*.{log,mem})" ] && exit 1
exit 0
