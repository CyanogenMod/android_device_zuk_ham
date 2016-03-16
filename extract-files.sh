#!/bin/sh

VENDOR=zuk
DEVICE=ham
PROPRIETARY_FILES=proprietary-files*.txt

BASE=../../../vendor/$VENDOR/$DEVICE/proprietary
rm -rf $BASE/*

for FILE in `cat $PROPRIETARY_FILES | grep -v ^# | grep -v ^$ `; do
    FILE=$(echo $FILE | sed -e 's|^-||g' | sed -e 's|^+||g')
    DIR=`dirname $FILE`
    if [ ! -d $BASE/$DIR ]; then
        mkdir -p $BASE/$DIR
    fi
    if [ "$#" -eq 1 ]; then
        cp $1/$FILE $BASE/$FILE
    else
        adb pull /system/$FILE $BASE/$FILE
    fi
done

./setup-makefiles.sh
