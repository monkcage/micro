

CMAKE_MINIMUM_REQUIRED(VERSION 3.8.0)

PROJECT(micro)

INCLUDE_DIRECTORIES(./th3party/inc ./src/core ./src)

LINK_DIRECTORIES(./th3party/lib)

ADD_DEFINITIONS(-std=c++11)

ADD_EXECUTABLE(client ./examples/client.cxx)
TARGET_LINK_LIBRARIES(client zmq)


ADD_EXECUTABLE(broker ./examples/broker.cxx 
                      ./src/core/ServiceCounter.cxx 
                      ./src/core/ServiceRouter.cxx
                      ./src/log/easylogging++.cc)
TARGET_LINK_LIBRARIES(broker zmq)


ADD_EXECUTABLE(server ./examples/service.cxx
                      ./src/IService.cxx
                      ./src/log/easylogging++.cc)
TARGET_LINK_LIBRARIES(server zmq)


ADD_EXECUTABLE(test ./examples/test.cxx 
                    ./src/IService.cxx
                    ./src/log/easylogging++.cc)
TARGET_LINK_LIBRARIES(test zmq)
