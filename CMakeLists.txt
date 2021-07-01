

CMAKE_MINIMUM_REQUIRED(VERSION 3.8.0)

PROJECT(micro)

INCLUDE_DIRECTORIES(./th3party/inc .)

LINK_DIRECTORIES(./th3party/lib)

ADD_DEFINITIONS(-std=c++11)

ADD_EXECUTABLE(client ./examples/client.cxx)
TARGET_LINK_LIBRARIES(client zmq)


ADD_EXECUTABLE(broker ./examples/broker.cxx)
TARGET_LINK_LIBRARIES(broker zmq)


ADD_EXECUTABLE(server ./examples/service.cxx)
TARGET_LINK_LIBRARIES(server zmq)
