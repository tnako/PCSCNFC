CONFIG -= qt
VPATH += ./src

DESTDIR = ./bin
OUT_PWD = ./bin

INCLUDEPATH += /usr/include/PCSC
LIBS += -pthread -lpcsclite

CONFIG(release, debug|release) {
    OBJECTS_DIR = ./build/release
    MOC_DIR = ./build/release
    RCC_DIR = ./build/release
    UI_DIR = ./build/release
    INCLUDEPATH += ./build/release
}

CONFIG(debug, debug|release) {
    TARGET = $$join(TARGET,,,d)
    OBJECTS_DIR = ./build/debug
    MOC_DIR = ./build/debug
    RCC_DIR = ./build/debug
    UI_DIR = ./build/debug
    INCLUDEPATH += ./build/debug
}

QMAKE_LINK = gcc
QMAKE_CFLAGS += -std=gnu11 -Wextra -Werror

QMAKE_CFLAGS_RELEASE += -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=2 -fstack-protector

QMAKE_LFLAGS += -Wl,--as-needed

SOURCES += \
    src/main.c

