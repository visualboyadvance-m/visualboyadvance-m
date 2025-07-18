set(PROJECT_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../..)

set(INCROOT ${PROJECT_SOURCE_DIR}/include/SFML/Network)
set(SRCROOT ${PROJECT_SOURCE_DIR}/src/SFML/Network)

# all source files
set(SRC
    ${INCROOT}/../Network.hpp
    ${INCROOT}/Export.hpp
    ${INCROOT}/Ftp.hpp
    ${INCROOT}/Http.hpp
    ${INCROOT}/IpAddress.hpp
    ${INCROOT}/Packet.hpp
    ${INCROOT}/SocketHandle.hpp
    ${INCROOT}/Socket.hpp
    ${INCROOT}/SocketSelector.hpp
    ${INCROOT}/TcpListener.hpp
    ${INCROOT}/TcpSocket.hpp
    ${INCROOT}/UdpSocket.hpp
    ${SRCROOT}/Ftp.cpp
    ${SRCROOT}/Http.cpp
    ${SRCROOT}/IpAddress.cpp
    ${SRCROOT}/Packet.cpp
    ${SRCROOT}/Socket.cpp
    ${SRCROOT}/SocketImpl.hpp
    ${SRCROOT}/SocketSelector.cpp
    ${SRCROOT}/TcpListener.cpp
    ${SRCROOT}/TcpSocket.cpp
    ${SRCROOT}/UdpSocket.cpp
)

# add platform specific sources
if(WIN32)
    set(SRC
        ${SRC}
        ${SRCROOT}/Win32/SocketImpl.cpp
    )
else()
    set(SRC
        ${SRC}
        ${SRCROOT}/Unix/SocketImpl.cpp
    )
endif()

source_group("" FILES ${SRC})

# define the sfml-network target
add_library(sfml-network ${SRC})

target_include_directories(sfml-network SYSTEM BEFORE PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../../../include)
target_include_directories(sfml-network PRIVATE              ${CMAKE_CURRENT_SOURCE_DIR}/../..)

target_compile_definitions(sfml-network PRIVATE   SFML_STATIC)
target_compile_definitions(sfml-network INTERFACE SFML_STATIC)

# setup dependencies
target_link_libraries(sfml-network PUBLIC sfml-system nonstd-lib ghc-filesystem)
if(WIN32)
    target_link_libraries(sfml-network PRIVATE ws2_32)
endif()
