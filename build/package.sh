#!/bin/bash

ARCH=""
VERSION=""
SRC=""
DST=""

while getopts "a:v:s:d:" arg
do
    case "$arg" in
        a)
            ARCH=$OPTARG
            ;;
        v)
            VERSION=$OPTARG
            ;;
        s)
            SRC=$OPTARG
            ;;
        d)
            DST=$OPTARG
            ;;
    esac
done

if [ -z "$ARCH" ] || [ -z "$VERSION" ] || [ -z "$SRC" ] || [ -z "$DST" ]; then
    echo "Usage: \$0 -a ARCH -v VERSION -s SRC -d DST"
    exit 1
fi

if [ -f "$SRC/axcl.tgz" ];then
    cat install.sh $SRC/axcl.tgz > $DST/axcl_"$VERSION"_linux_"$ARCH".run  
else
    echo "Not found $SRC/axcl.tgz file!."
    exit 1
fi

chmod +x $DST/axcl_"$VERSION"_linux_"$ARCH".run

echo "$DST/axcl_"$VERSION"_linux_"$ARCH".run is created."
