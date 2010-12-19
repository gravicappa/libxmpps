name = myxml
CC = pcc
LIBS =
CFLAGS = -O0 -g -Wall -pedantic
LDFLAGS = 
LANG = C

default:V: test_xml

test:V: $name
	ulimit -c 65536
	#./$name < test/test.xml
	./$name < test/stream_end.xml

test_xml: xml.o pool.o node.o input.o test_xml.o
	$CC $CFLAGS $prereq $LDFLAGS $LIBS -o $target

%.o: %.c
	$CC $CFLAGS -c $stem.c $LIBS -o $target

clean:V:
	rm -rf $name *.o
