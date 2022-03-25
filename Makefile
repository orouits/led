CC            = gcc
DEFINES       = 
CFLAGS        = -pipe -O2 -Wall -Wextra -fPIC $(DEFINES)
INCPATH       = -I.
LINK          = g++
LFLAGS        = -Wl,-O1
AR            = ar cqs
RANLIB        = 
SED           = sed
STRIP         = strip

####### Output directory


SOURCES       = led.c 
OBJECTS       = led.o
LIBS          = -lpcre2-8   
DESTDIR       = 
TARGET        = led
DISTNAME      = led-1.0.0
DISTDIR       = /home/ol/src/led/$(DISTNAME)

####### Build rules

led:  $(OBJECTS)  
	$(LINK) $(LFLAGS) -o $(TARGET) $(OBJECTS) $(OBJCOMP) $(LIBS)

all: led

clean:
	rm *.o led

distclean: clean

####### Sub-libraries

test:
	./test.sh

####### Compile

led.o: led.c 
	$(CC) -c $(CFLAGS) $(INCPATH) -o led.o led.c

####### Install

install:

uninstall:
