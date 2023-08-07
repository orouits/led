CC            = gcc
DEFINES       =
CFLAGS        = -pipe -O2 -Wall -Wextra -fPIC $(DEFINES)
INCPATH       = -I.
LINK          = g++
LFLAGS        = -Wl,-O1

####### Output directory


SOURCES       = *.c
OBJECTS       = led.o led_fn.o led_err.o led_str.o
LIBS          = -lpcre2-8 -lb64
DESTDIR       = .
TARGET        = led
DISTNAME      = led-1.0.0
DISTDIR       = /home/ol/src/led/$(DISTNAME)

####### Build rules

led:  $(OBJECTS)
	$(LINK) $(LFLAGS) -o $(TARGET) $(OBJECTS) $(OBJCOMP) $(LIBS)

all: led

clean:
	rm -f *.o led

distclean: clean

####### actons

test: FORCE
	test/test.sh

FORCE:

####### Compile

led.o: led.c led.h
	$(CC) -c $(CFLAGS) $(INCPATH) -o led.o led.c

led_fn.o: led_fn.c led.h
	$(CC) -c $(CFLAGS) $(INCPATH) -o led_fn.o led_fn.c

led_err.o: led_err.c led.h
	$(CC) -c $(CFLAGS) $(INCPATH) -o led_err.o led_err.c

led_str.o: led_str.c led.h
	$(CC) -c $(CFLAGS) $(INCPATH) -o led_str.o led_str.c

####### Install

install:

uninstall:
