#!/bin/bash
#this is no way a standard configure script, this just check dependencies
# and re-creates Makefile
# 



#Critical dependencies
function check2 {
	g++ -M cphix2.cpp 2>&1 | grep -q $1  || { echo "$1 missing, install $2....";exit 1;}
	echo "Checking for "$1" ... yes"
	}


check2 CImg.h "Cimg (C Imaging library)"




COMMAND="g++ cphix2.cpp -L/usr/lib "

function check {
	result=no
	g++ -l$1 2>&1 | grep -q -i "Undefined reference"  && result=yes
	echo "Checking for "$1" ...  "$result
	
	
	}



check pthread
if [ "$result" == "yes" ] ; then
COMMAND="$COMMAND -lpthread "
fi
check m
if [ "$result" == "yes" ] ; then
COMMAND="$COMMAND -lm "
fi
check png
if [ $result == "yes" ] ; then
COMMAND="$COMMAND -lpng  -Dcimg_use_png "
fi
check jpeg
if [ $result == "yes" ] ; then
COMMAND="$COMMAND -ljpeg  -Dcimg_use_jpeg "
fi
#check png
#if [ $result == "yes" ] ; then
#COMMAND="$COMMAND -lpng  -Dcimg_use_png "
#fi
check tiff
if [ $result == "yes" ] ; then
COMMAND="$COMMAND -ltiff  -Dcimg_use_tiff "
fi
check z
if [ $result == "yes" ] ; then
COMMAND="$COMMAND -lz  -Dcimg_use_zlib "
fi
check zma
if [ $result == "yes" ] ; then
COMMAND="$COMMAND -I/usr/include/lzma "
fi
check X11
if [ $result == "yes" ] ; then
COMMAND="$COMMAND -L/usr/X11R6/lib -lX11 -Dcimg_display=1 "
else
COMMAND="$COMMAND -Dcimg_display=0 "
fi

COMMAND="$COMMAND -I/usr/include -O3 -fno-tree-pre -o cphix2.bin easyexif/exif.o"
echo -e "... OK, ready for compilation. COMPILATION COMMAND:"
echo $COMMAND


echo "all: cphix2.bin

exif.o: easyexif/exif.cpp
	g++ -O2 -pedantic -Wall -Wextra -ansi -c easyexif/exif.cpp -o easyexif/exif.o

cphix2.bin:cphix2.cpp easyexif/exif.o">Makefile
echo "	$COMMAND" >> Makefile

echo "
wall:cphix2.cpp easyexif/exif.o">>Makefile
echo "	"`echo $COMMAND | sed 's:cphix2.cpp:cphix2.cpp -Wall:'`>>Makefile

echo "

clean:
	rm -f easyexif/exif.o cphix2.bin" >> Makefile
	
exit 0




