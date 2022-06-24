#!/bin/bash

SCRIPT_DIR=$(cd $(dirname $0); pwd)

cat $SCRIPT_DIR/test1.txt | ./led AA