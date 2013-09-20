# this is to compile cphix - photo normalizing CLI tool
all: cphix2.bin

exif.o: easyexif/exif.cpp
	g++ -O2 -pedantic -Wall -Wextra -ansi -c easyexif/exif.cpp

cphix2.bin:cphix2.cpp easyexif/exif.o
	g++ cphix2.cpp -O2 -L/usr/X11R6/lib -lm -lpthread -lX11 -o cphix2.bin easyexif/exif.o
