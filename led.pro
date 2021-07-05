TEMPLATE = app
CONFIG += console
CONFIG -= app_bundle
CONFIG -= qt

SOURCES += \
        led.c

DISTFILES += \
    LICENSE \
    README.md

LIBS += -lpcre2-8
