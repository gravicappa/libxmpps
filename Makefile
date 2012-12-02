name = libxmpps
TARG = $(name).a
O = o
LIBFILES = xml.o fsm.o pool.o node.o xmpp.o md5.o base64.o tls.o

include config.mk

CFLAGS += -Os

all: $(TARG) sjc

clean:
	rm -f *.o test_xml test_xmpp $(name).a sjc

$(TARG): $(LIBFILES)
	ar rcu $@ $(LIBFILES)

test_xml: test_xml.$O xml.$O fsm.$O pool.$O node.$O

test_xmpp: test_xmpp.$O $(LIBFILES)

test_base64: test_base64.o base64.o

sjc: sjc.$O $(name).a

xml.$O: xml.c xml_states.h

%: %.$O
	$(CC) $(CFLAGS) -DVERSION=\"$(VERSION)\" -o $@ $^ $(LDFLAGS)

%.$O: %.c
	$(CC) $(CFLAGS) -DVERSION=\"$(VERSION)\" -c $< -o $@

install: sjc $(TARG)
	mkdir -p "$(destdir)/usr/include/libxmpps/" "$(destdir)/usr/lib/"
	mkdir -p "$(destdir)/usr/bin/"
	cp *.h "$(destdir)/usr/include/libxmpps/"
	cp $(TARG) "$(destdir)/usr/lib/"
	cp sjc "$(destdir)/usr/bin/"
