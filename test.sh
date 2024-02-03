#!/bin/bash

SCRIPT_DIR=$(cd $(dirname $0); pwd)
TEST_DIR=$SCRIPT_DIR/test

TEST=${1:-all}

echo -e "\ncleanup data:"

rm -rf $TEST_DIR/files_*
mkdir -p $TEST_DIR/files_in
mkdir -p $TEST_DIR/files_out

echo -e "\nprepare data:"

cat - > $TEST_DIR/files_in/file_1<<EOT
sdvmiksfqs
TEST 11111111
SLFKSLDkfj
111111 TEST SDFMLSDF
dfsldkfjsldf
EOT

cat - > $TEST_DIR/files_in/file_2<<EOT
2222222é
TEST 22222222
SLFKSLDkfj
222222 TEST SDFMLSDF
dfsldkfjsldf
EOT

echo -e "\ntest = $TEST"

if [[ $TEST == 1 || $TEST == all ]]; then
    echo -e "\ntest 1:"
    cat $TEST_DIR/files_in/file_1 | ./led '111*'
fi

if [[ $TEST == 2 || $TEST == all ]]; then
    echo -e "\ntest 2:"
    ls $TEST_DIR/files_in/* | led -v TEST -f
fi

if [[ $TEST == 3 || $TEST == all ]]; then
    echo -e "\ntest 3:"
    ls $TEST_DIR/files_in/* | led -v TEST -F -f
fi

if [[ $TEST == 4 || $TEST == all ]]; then
    echo -e "\ntest 4:"
    ls $TEST_DIR/files_in/* | led -v TEST -W$TEST_DIR/files_out/write -f
fi

if [[ $TEST == 5 || $TEST == all ]]; then
    echo -e "\ntest 5:"
    ls $TEST_DIR/files_in/* | led -v TEST -A$TEST_DIR/files_out/append -f
fi

if [[ $TEST == 6 || $TEST == all ]]; then
    echo -e "\ntest 6:"
    ls $TEST_DIR/files_in/* | led TEST -D$TEST_DIR/files_out -f
fi

echo -e "\nfiles_out:"
ls -l $TEST_DIR/files_out/*
