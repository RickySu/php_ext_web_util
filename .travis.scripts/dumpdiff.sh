#!/bin/bash
n=`find tests/|grep .diff$|wc -l`
if test "$n" != "0"; then
    cat tests/*.diff
fi
