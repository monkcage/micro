TEMPLATE = app
CONFIG += console c++11
CONFIG -= app_bundle
CONFIG -= qt


INCLUDEPATH += $$PWD/th3party/inc


HEADERS += \
    ServiceCounter.hpp

SOURCES += \
    examples/broker.cxx \
    examples/client.cxx \
    examples/service.cxx

