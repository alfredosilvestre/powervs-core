TEMPLATE = lib

TARGET = PowerVSCore

CONFIG += debug_and_release

# Add the headers to the include path
INCLUDEPATH += decklink_sdk/ include include/decklink include/licensing include/player include/recorder include/videoport log4cxx/include

# Required for some C99 defines
DEFINES += __STDC_CONSTANT_MACROS

# Define features to use
DEFINES += DECKLINK

# Add the headers to the include path
INCLUDEPATH += ffmpeg/include

# Add the path to static libraries
LIBS += -Lffmpeg/lib

# Add the path to shared libraries
LIBS += -Lffmpeg/shared

# Log4CXX is a special cookie :)
CONFIG(debug, debug|release){
    LIBS += -Llog4cxx/Debug/
} else {
    LIBS += -Llog4cxx/lib/ -Llog4cxx/shared/
}

# Set list of required libraries
LIBS += -lcomsuppw -lavcodec-57 -lavformat-57 -lavutil-55 -lswscale-4 -lswresample-2 -lavfilter-6 -llog4cxx

SOURCES += \
    decklink_sdk/DeckLinkAPI_i.c \
    src/decklink/decklinkinput.cpp \
    src/decklink/decklinkoutput.cpp \
    src/decklink/decklinkcontrol.cpp \
    src/player/audiosamplearray.cpp \
    src/player/audiothread.cpp \
    src/player/avdecodedframe.cpp \
    src/player/ffdecoder.cpp \
    src/player/player.cpp \
    src/player/videothread.cpp \
    src/recorder/videoencoder.cpp \
    src/recorder/recorder.cpp \
    src/recorder/muxer.cpp \
    src/recorder/audioencoder.cpp \
    src/videoport/videoport.cpp \
    src/iobridge.cpp \
    src/core.cpp \
    src/main.cpp

HEADERS += \
    decklink_sdk/DeckLinkAPI_h.h \
    include/decklink/decklinkinput.h \
    include/decklink/decklinkoutput.h \
    include/decklink/decklinkcontrol.h \
    include/player/audiosamplearray.h \
    include/player/audiothread.h \
    include/player/avdecodedframe.h \
    include/player/ffdecoder.h \
    include/player/player.h \
    include/player/videothread.h \
    include/recorder/videoencoder.h \
    include/recorder/recorder.h \
    include/recorder/muxer.h \
    include/recorder/audioencoder.h \
    include/videoport/videoport.h \
    include/iobridge.h \
    include/core.h
