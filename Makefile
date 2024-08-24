####### Tools and options

CC            = gcc
DEFINES       =
CFLAGS        = -pipe -O2 -Wall -Wextra -fPIC $(DEFINES)
LINK          = g++
LFLAGS        = -Wl,-O1

####### Application and dirs

MAKEFILE    = $(lastword $(MAKEFILE_LIST))
SOURCEDIR 	= $(dir $(realpath $(MAKEFILE)))
SOURCES     = $(wildcard *.c)
OBJECTS		= $(patsubst %.c,%.o,$(SOURCES))
APP			= led
APPTEST 	= $(APP)test
ARCNAME		= $(APP)_bin.tgz
LIBS        = -lpcre2-8 -lb64
VERSION     = 1.0.0
INSTALLDIR  = /usr/local/bin/

####### Build rules

all: $(APP) $(APPTEST) $(HOME)/.local/bin/$(APP) VERSION

%.o : %.c $(APP).h
	$(CC) -c $(CFLAGS) -I$(SOURCEDIR) $< -o $@

$(APP): $(filter-out $(APPTEST).o, $(OBJECTS))
	$(LINK) $(LFLAGS) -o $@ $^ $(LIBS)

$(APPTEST): $(filter-out $(APP).o, $(OBJECTS))
	$(LINK) $(LFLAGS) -o $@ $^ $(LIBS)

VERSION: $(MAKEFILE)
	echo $(VERSION) > $@

$(HOME)/.local/bin/$(APP):
	mkdir -p $(HOME)/.local/bin
	ln -s -f $(SOURCEDIR)$(APP) $@

clean:
	rm -f *.o $(APP) $(APP)test
	rm -f ~/.local/bin/$(APP)
	rm -f *.tgz

distclean: clean

####### Test

.PHONY: test
test: $(APP)
	./test.sh

####### Install an packaging

install: $(APP)
	sudo mkdir -p $(INSTALLDIR)
	sudo cp $(SOURCEDIR)$(APP) $(INSTALLDIR)$(APP)

uninstall:
	sudo rm -f $(INSTALLDIR)$(APP)

deb: $(APP)
	#not implemented

$(ARCNAME): $(APP)
	rm -f $(ARCNAME)
	tar -czf $(ARCNAME) $(APP)

release: $(ARCNAME) VERSION
	git pull
	git add * || true
	git commit -a -m "prepare release $(VERSION)"
	git tag -a $(VERSION) -m "release $(VERSION)"
	git push --tags

publish: clean release
	#not implemented
