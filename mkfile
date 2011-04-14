name = libxmpps
TARG = $name.a
O = o
LIBFILES = xml.o fsm.o pool.o node.o xmpp.o md5.o base64.o tls.o

< config.mk

CFLAGS = $CFLAGS -O0 -g -Wall -pedantic

default:V: $TARG sjc

clean:V:
  rm -f *.o test_xml test_xmpp $name.a sjc

$name.a(%.$O):N: %.$O

$name.a: $LIBFILES
	ar rcu $target $prereq

test_xml: test_xml.$O xml.$O fsm.$O pool.$O node.$O

test_xmpp: test_xmpp.$O $LIBFILES

test_base64: test_base64.o base64.o

sjc: sjc.$O $name.a

xml.$O: xml.c xml_states.h

xml_states.h: xml.c
  awk < $prereq '!inside && /^#define RULE/ { inside = 1; print; next; }
                 inside && /^};$/ { inside = 0; print; }
                 inside { print; }' \
  | $CC -E - \
  | awk 'BEGIN {RS="{"; FS="[ ,]"}
			   /^[A-Z]/ {
           if (!items[$1]) {
             printf("#define %s %d\n", $1, nitems++);
             items[$1] = nitems;
           }
         }' \
  > $target

%: %.$O
  $CC $CFLAGS -o $target $prereq $LDFLAGS

%.$O: %.c
  $CC $CFLAGS -c $stem.c -o $target

install:V: sjc $TARG
	mkdir -p "$destdir/usr/include/libxmpps/" "$destdir/usr/lib/"
	mkdir -p "$destdir/usr/bin/"
	cp *.h "$destdir/usr/include/libxmpps/"
	cp $TARG "$destdir/usr/lib/"
	cp sjc "$destdir/usr/bin/"
