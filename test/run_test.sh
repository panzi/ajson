#!/bin/bash

RED="\e[31m"
GREEN="\e[32m"
NORMAL="\e[0m"

OK="[   ${GREEN}OK${NORMAL}   ]"
FAILED="[ ${RED}FAILED${NORMAL} ]"

if [ $1 = --memcheck ]; then
	TOKENS="valgrind -q --leak-check=full ./tokens --read=fread"
	shift
else
	TOKENS="./tokens --read=fread"
fi

SRC_DIR=`dirname $0`
BIN_DIR=$1

exit_status=0
tests=0
success=0
for cases in positive negative; do
	echo
	echo "========== $cases ==========="
	dir_tests=0
	dir_success=0
	mkdir -p "$BIN_DIR/data/$cases"
	for json in "$SRC_DIR/data/$cases"/*.json; do
		test_case=`basename "$json" .json`
		tokens="$SRC_DIR/data/$cases/${test_case}.tokens"
		out="$BIN_DIR/data/$cases/${test_case}.out"
		printf "[        ] %s" "$test_case"
		test_ok=true
		for size in 1 8192; do
			$TOKENS --buffer-size=$size "$json" > "$out" 2>/dev/null
			
			if ! cmp -s "$tokens" "$out"; then
				test_ok=false
				break
			fi
			
			rm "$out"
		done
		if [ $test_ok = true ]; then
			printf "\r$OK %s\n" "$test_case"
			dir_success=$((dir_success+1))
		else
			printf "\r$FAILED %s\n" "$test_case"
			exit_status=1
		fi
		dir_tests=$((dir_tests+1))
	done

	echo "$dir_success successful, $(($dir_tests-$dir_success)) failed"

	tests=$((tests+dir_tests))
	success=$((success+dir_success))
done

echo
echo "summary:"
echo "$success successful, $(($tests-$success)) failed"
echo

exit $exit_status
