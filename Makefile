# Copyright (C) 2004 Peter Urbanec
# $Id: Makefile,v 1.1 2004/12/08 12:55:11 purbanec Exp $

ifdef CROSS

#
# TODO: This is just a temporary hack to crosscompile on my machine.
#       What's needed is a proper cross-build script for unslung / OE
#
MY_CROSS_HOME=/home/slug/bk/build/tmp/staging/armeb-linux

ARCH="arm"

CROSS_COMPILE="armeb-linux-"
AR="armeb-linux-ar"
CPP="armeb-linux-gcc -E"
CC="armeb-linux-gcc"
CXX="armeb-linux-g++"
LD="armeb-linux-ld"
CCLD="armeb-linux-gcc"
STRIP="armeb-linux-strip"
RANLIB="armeb-linux-ranlib"

CPPFLAGS="-I${MY_CROSS_HOME}/include"
CFLAGS="-Wall -I${MY_CROSS_HOME}/include -fexpensive-optimizations -fomit-frame-pointer -frename-registers -O2"
CXXFLAGS="-Wall -I${MY_CROSS_HOME}/include -fexpensive-optimizations -fomit-frame-pointer -frename-registers -O2 -fpermissive"
LDFLAGS=-L${MY_CROSS_HOME}/lib -Wl,-rpath-link,${MY_CROSS_HOME}/lib -Wl,-rpath,/usr/lib -Wl,-O2

else
CFLAGS="-Wall -fexpensive-optimizations -fomit-frame-pointer -frename-registers -O2"
CXXFLAGS="-Wall -fexpensive-optimizations -fomit-frame-pointer -frename-registers -O2 -fpermissive"
LDFLAGS=-Wl,-O2
endif


puppy: puppy.o crc16.o usb_io.o tf_bytes.o

puppy.o: puppy.c crc16.h usb_io.h tf_bytes.h

tf_bytes.o: tf_bytes.h tf_bytes.c

crc16.o: crc16.h crc16.c

usb_io.o: usb_io.h usb_io.h crc16.h tf_bytes.h

clean:
	-rm -f *.o
	-rm -f *~
	-rm -f puppy

