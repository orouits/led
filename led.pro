TEMPLATE = app
CONFIG += console
CONFIG -= app_bundle
CONFIG -= qt

SOURCES += \
        led.c

DISTFILES += \
    .gitignore \
    LICENSE \
    README.md \
    test/test1.txt

LIBS += -lpcre2-8
