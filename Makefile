CC            = gcc
DEFINES       =
CFLAGS        = -pipe -O2 -Wall -Wextra -fPIC $(DEFINES)
INCPATH       = -I.
LINK          = g++
LFLAGS        = -Wl,-O1

####### Output directory

SOURCEDIR 	= $(dir $(realpath $(lastword $(MAKEFILE_LIST))))
SOURCES     = $(wildcard *.c)
OBJECTS		= $(patsubst %.c,%.o,$(SOURCES))
EXECUTABLE	= led
LIBS        = -lpcre2-8 -lb64
VERSION     = 1.0.0
INSTALLDIR  = /usr/local/bin/


####### Build rules

%.o : %.c $(EXECUTABLE).h
	$(CC) -c $(CFLAGS) $(INCPATH) $< -o $@

$(EXECUTABLE): $(OBJECTS)
	$(LINK) $(LFLAGS) -o $@ $? $(LIBS)
	mkdir -p ~/.local/bin
	ln -s -f $(SOURCEDIR)$(EXECUTABLE) ~/.local/bin/$(EXECUTABLE)

all: $(EXECUTABLE)

clean:
	rm -f *.o $(EXECUTABLE)
	rm -f ~/.local/bin/$(EXECUTABLE)

distclean: clean

####### Test

.PHONY: test
test: $(EXECUTABLE)
	./test.sh

####### Install

install: $(EXECUTABLE)
	sudo mkdir -p $(INSTALLDIR)
	sudo cp $(SOURCEDIR)$(EXECUTABLE) $(INSTALLDIR)$(EXECUTABLE)

uninstall:
	rm -f $(INSTALLDIR)$(EXECUTABLE)
