TARG = test_fsm
CC = pcc
CFLAGS = -O0 -g -Wall -pedantic

< config.mk

test:V: test_xml
	ulimit -c 65536
	./test_xml < test/xmpp1.xml
	ulimit -c 0

test_xml: test_xml.o xml.o fsm.o pool.o node.o

xml.o: xml.c xml_states.h

states:V: xml.c
  awk < $prereq '!inside && /^#define RULE/ { inside = 1; print; next; }
                 inside && /^};$/ { inside = 0; print; }
                 inside { print; }' \
  | $CC -E - | sed 's/},/&\n/g'

xml_states.h: xml.c
  awk < $prereq '!inside && /^#define RULE/ { inside = 1; print; next; }
                 inside && /^};$/ { inside = 0; print; }
                 inside { print; }' \
  | $CC -E - | sed 's/},/&\n/g' \
  | awk 'BEGIN { FS="[ ,]" }
	       /^struct fsm_rule xml_rules\[\]/ { inside = 1; }
         inside && /^};$/ { inside = 0; }
         inside {
				 	 sub(/^[ 	]*{/,"");
				 	 sub(/^[^,]*$/,"");
					 if (match($1, /^XML_STATE/) && !items[$1]) {
						 printf("#define %s %d\n", $1, nitems++);
						 items[$1] = nitems;
					 }
         }' \
	> $target

%: %.o
	$CC $CFLAGS -o $target $prereq 

%.o: %.c
	$CC $CFLAGS -c $stem.c -o $target
