
VERSION=0.27
BUILDTIME=$(shell date "+%Y%m%d-%H%M")

#This will choke on a non-debian system
ARCH=$(shell dpkg --print-architecture)
#ARCH=i386


STRIP= strip --remove-section=.note --remove-section=.comment busybox
LDFLAGS= -s

CFLAGS=-Wall -O2 -fomit-frame-pointer -fno-builtin -D_GNU_SOURCE
LIBRARIES=-lc
OBJECTS=$(shell ./busybox.obj)

CFLAGS+= -DBB_VER='"$(VERSION)"'
CFLAGS+= -DBB_BT='"$(BUILDTIME)"'

# -D_GNU_SOURCE is needed because environ is used in init.c
ifdef INCLUDE_DINSTALL
  CFLAGS+= -DINCLUDE_DINSTALL
  LIBRARIES+= -lnewt -lslang
ifeq ($(ARCH),sparc)
  LIBRARIES+= -lm
endif
  OBJECTS+= $(patsubst %.c,%.o,$(wildcard ../dinstall/*.c)) \
	  ../libfdisk/libfdisk.a
endif

all: busybox links

busybox: $(OBJECTS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o busybox $(OBJECTS) $(LIBRARIES)
	$(STRIP)

links:
	- ./busybox.mkll | sort >busybox.links
	
../libfdisk/libfdisk.a: force
	$(MAKE) -C ../libfdisk libfdisk.a

clean:
	- rm -f busybox busybox.links *~ *.o 

distclean: clean
	- rm -f busybox

force:
