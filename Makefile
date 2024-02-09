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
APP			= led
LIBS        = -lpcre2-8 -lb64
VERSION     = 1.0.0
INSTALLDIR  = /usr/local/bin/

####### Build rules

%.o : %.c $(APP).h
	$(CC) -c $(CFLAGS) $(INCPATH) $< -o $@

$(APP): $(OBJECTS)
	$(LINK) $(LFLAGS) -o $@ $? $(LIBS)
	mkdir -p ~/.local/bin
	ln -s -f $(SOURCEDIR)$(APP) ~/.local/bin/$(APP)

all: $(APP)

clean:
	rm -f *.o $(APP)
	rm -f ~/.local/bin/$(APP)

distclean: clean

####### Test

.PHONY: test
test: $(APP)
	./test.sh

####### Install

install: $(APP)
	sudo mkdir -p $(INSTALLDIR)
	sudo cp $(SOURCEDIR)$(APP) $(INSTALLDIR)$(APP)

uninstall:
	sudo rm -f $(INSTALLDIR)$(APP)

deb: $(APP)
	# not implemented