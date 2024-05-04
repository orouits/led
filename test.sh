#!/bin/bash

SCRIPT_DIR=$(cd $(dirname $0); pwd)
TEST_DIR=$SCRIPT_DIR/test

TEST=${1:-all}

# echo -e "\nmake:"

# make || exit 1

echo -e "\ncleanup data:"

rm -rf $TEST_DIR/*
mkdir -p $TEST_DIR/files_in
mkdir -p $TEST_DIR/files_out

echo -e "\nprepare data:"

cat - > $TEST_DIR/files_in/file_0<<EOT
AAA

AAA 222
CCC 222
TEST AAA
EOT

cat - > $TEST_DIR/files_in/file_1<<EOT
sdvmiksfqs
TEST 11111111
SLFKSLDkfj
111111 TEST SDFMLSDF
dfsldkfjsldf
EOT

cat - > $TEST_DIR/files_in/file_2<<EOT
2222222
TEST 22222222
SLFKSLDkfj
222222 TEST SDFMLSDF
dfsldkfjsldf
EOT

cat - > $TEST_DIR/files_in/file_pass<<EOT
# user
app1_user: test # no white chars allowed
app2_user: test # no white chars allowed

# password
app1_pwd: "_super_P@ssW0rd_" # strong password > 16
app2_pwd: "_super_P@ssW0rd_" # strong password > 16
EOT

cat - > $TEST_DIR/files_out/append<<EOT
EXISTING LINE 1
EXISTING LINE 2
EOT

cat - > $TEST_DIR/files_out/write<<EOT
EXISTING LINE 1
EXISTING LINE 2
EOT

mkdir -p $TEST_DIR/files_to_mv
touch $TEST_DIR/files_to_mv/file1\ to\'\ mv.txt
touch $TEST_DIR/files_to_mv/file2\ to\'\ mv.txt

echo -e "\ntest = $TEST"

if [[ $TEST == 1 || $TEST == all ]]; then
    echo -e "\ntest 1:"
    cat $TEST_DIR/files_in/file_0 | ./led -v AA
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
    ls $TEST_DIR/files_in/* | led -v TEST -D$TEST_DIR/files_out -f
fi

if [[ $TEST == 7 || $TEST == all ]]; then
    echo -e "\ntest 7:"
    ls $TEST_DIR/files_in/file_0 | led -v AA -W$TEST_DIR/files_out/file_0 -f | led -v 'TE.*' -W$TEST_DIR/files_out/file_02 -f
fi

if [[ $TEST == 9 || $TEST == all ]]; then
    echo -e "\ntest 9:"
    ls $TEST_DIR/files_in/* | led -v AA -E.out1 -f | led -v 'TE.*' -E.out2 -f
fi

if [[ $TEST == 10 || $TEST == all ]]; then
    echo -e "\ntest 10:"
    ls $TEST_DIR/files_in/file_pass | led -v _pwd -E.enc 'b64e/_pwd: "(.+)"/' -f | led -v _pwd -E.dec 'b64d/_pwd: "(.+)"/' -f
fi

if [[ $TEST == 11 || $TEST == all ]]; then
    echo -e "\ntest 11:"
    ls $TEST_DIR/files_in/* | led _pwd -F 'b64e/(.+)' -f | led -r _pwd -E.dec 'b64d/(.+)' -f
fi

if [[ $TEST == 12 || $TEST == all ]]; then
    echo -e "\ntest 12:"
    ls $TEST_DIR/files_in/* | led -v r// fnu// 's//$R $0/'
fi

if [[ $TEST == 13 || $TEST == all ]]; then
    echo -e "\ntest 13:"
    ls $TEST_DIR/files_to_mv/* | led -v she// r// shu// fnc// 's//mv $R $0/' -X
fi

echo -e "\nfiles:"
ls -1 $TEST_DIR/files_in/*
ls -1 $TEST_DIR/files_out/*
ls -1 $TEST_DIR/files_to_mv/*
