#!/bin/sh

VENDOR=zuk
DEVICE=ham

BASE=../../../vendor/$VENDOR/$DEVICE/proprietary
rm -rf $BASE/*

for FILE in `cat proprietary-files-qc.txt proprietary-files.txt | grep -v ^# | grep -v ^$ `; do
    FILE=$(echo $FILE | sed -e 's|^-||g')
    DIR=`dirname $FILE`
    if [ ! -d $BASE/$DIR ]; then
        mkdir -p $BASE/$DIR
    fi
    adb pull /system/$FILE $BASE/$FILE
done

./setup-makefiles.sh
