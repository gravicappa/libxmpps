name = libxmpp
TARG = $name.a
CC = pcc
O = o
LIBFILES = xml.o fsm.o pool.o node.o xmpp.o md5.o base64.o

< config.mk

CFLAGS = $CFLAGS -O0 -g -Wall -pedantic

default:V: sjc

run_xmpp_test:V: test_xmpp
	rm -f core
  ulimit -c 65536
	#./test_xmpp < test/xmpp1.xml
	./test_xmpp < test/sasl.xml
  ulimit -c 0

run_xml_test:V: test_xml
	rm -f core
  ulimit -c 65536
  ./test_xml < test/in.xmpp.xml
  ulimit -c 0

clean:V:
  rm -f *.o test_xml test_xmpp $name.a sjc

$name.a(%.$O):N: %.$O

$name.a: $LIBFILES $TLS_OBJ
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
