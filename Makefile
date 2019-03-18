# Makefile for busybox
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
# General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
#


PROG=busybox
VERSION=0.33
BUILDTIME=$(shell date "+%Y%m%d-%H%M")

# Comment out the following to make a debuggable build
# Leave this off for production use.
DODEBUG=false
# If you want a static binary, turn this on.  I can't think
# of many situations where anybody would ever want it static, 
# but...
DOSTATIC=false

#This will choke on a non-debian system
ARCH=`uname -m | sed -e 's/i.86/i386/' | sed -e 's/sparc.*/sparc/'`


# -D_GNU_SOURCE is needed because environ is used in init.c
ifeq ($(DODEBUG),true)
    CFLAGS=-Wall -g -D_GNU_SOURCE -DDEBUG_INIT
    STRIP=
    LDFLAGS=
else
    CFLAGS=-Wall -Os -fomit-frame-pointer -fno-builtin -D_GNU_SOURCE
    LDFLAGS= -s
    STRIP= strip --remove-section=.note --remove-section=.comment $(PROG)
    #Only staticly link when _not_ debugging 
    ifeq ($(DOSTATIC),true)
	LDFLAGS+= --static
    endif
    
endif

ifndef $(prefix)
    prefix=`pwd`
endif
BINDIR=$(prefix)

LIBRARIES=
OBJECTS=$(shell ./busybox.sh)
CFLAGS+= -DBB_VER='"$(VERSION)"'
CFLAGS+= -DBB_BT='"$(BUILDTIME)"'

all: busybox links

busybox: $(OBJECTS)
	$(CC) $(LDFLAGS) -o $(PROG) $(OBJECTS) $(LIBRARIES)
	$(STRIP)

links:
	- ./busybox.mkll | sort >busybox.links
	
clean:
	- rm -f $(PROG) busybox.links *~ *.o core 

distclean: clean
	- rm -f $(PROG)

force:

$(OBJECTS):  busybox.def.h internal.h Makefile

install:    $(PROG)
	install.sh $(BINDIR)

