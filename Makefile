# Copyright (C) 2004 Peter Urbanec
# $Id: Makefile,v 1.3 2004/12/15 02:46:06 purbanec Exp $

ifdef CROSS

ARCH=armv5b-softfloat-linux
CROSS_HOME=/opt/crosstool/${ARCH}/gcc-3.3.4-glibc-2.2.5
CROSS_PATH=${CROSS_HOME}/bin

AR=${CROSS_PATH}/${ARCH}-ar
CPP=${CROSS_PATH}/${ARCH}-gcc -E
CC=${CROSS_PATH}/${ARCH}-gcc
CXX=${CROSS_PATH}/${ARCH}-g++
LD=${CROSS_PATH}/${ARCH}-ld
CCLD=${CROSS_PATH}/${ARCH}-gcc
STRIP=${CROSS_PATH}/${ARCH}-strip
RANLIB=${CROSS_PATH}/${ARCH}-ranlib

LDFLAGS=-L${CROSS_HOME}/lib -Wl,-rpath-link,${CROSS_HOME}/lib -Wl,-rpath,/usr/lib -Wl,-O2

else

LDFLAGS=-Wl,-O2

endif

CFLAGS=-std=gnu99 -Wall -W -Wshadow -Wstrict-prototypes -pedantic -fexpensive-optimizations -fomit-frame-pointer -frename-registers -O2


puppy: puppy.o crc16.o usb_io.o tf_bytes.o

puppy.o: puppy.c crc16.h usb_io.h tf_bytes.h

tf_bytes.o: tf_bytes.h tf_bytes.c

crc16.o: crc16.h crc16.c

usb_io.o: usb_io.h usb_io.h crc16.h tf_bytes.h

clean:
	-rm -f *.o
	-rm -f *~
	-rm -f puppy

