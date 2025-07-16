set(PROJECT_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../..)

set(INCROOT ${PROJECT_SOURCE_DIR}/include/SFML/System)
set(SRCROOT ${PROJECT_SOURCE_DIR}/src/SFML/System)

# all source files
set(SRC
    ${INCROOT}/../System.hpp
    ${INCROOT}/Angle.hpp
    ${INCROOT}/Angle.inl
    ${INCROOT}/Clock.hpp
    ${INCROOT}/Err.hpp
    ${INCROOT}/Exception.hpp
    ${INCROOT}/Export.hpp
    ${INCROOT}/FileInputStream.hpp
    ${INCROOT}/InputStream.hpp
    ${INCROOT}/MemoryInputStream.hpp
    ${INCROOT}/NativeActivity.hpp
    ${INCROOT}/Sleep.hpp
    ${INCROOT}/String.hpp
    ${INCROOT}/String.inl
    ${INCROOT}/SuspendAwareClock.hpp
    ${INCROOT}/Time.hpp
    ${INCROOT}/Time.inl
    ${INCROOT}/Utf.hpp
    ${INCROOT}/Utf.inl
    ${INCROOT}/Vector2.hpp
    ${INCROOT}/Vector2.inl
    ${INCROOT}/Vector3.hpp
    ${INCROOT}/Vector3.inl
    ${SRCROOT}/Clock.cpp
    ${SRCROOT}/EnumArray.hpp
    ${SRCROOT}/Err.cpp
    ${SRCROOT}/FileInputStream.cpp
    ${SRCROOT}/MemoryInputStream.cpp
    ${SRCROOT}/Sleep.cpp
    ${SRCROOT}/String.cpp
    ${SRCROOT}/Utils.cpp
    ${SRCROOT}/Utils.hpp
    ${SRCROOT}/Vector2.cpp
    ${SRCROOT}/Vector3.cpp
)
source_group("" FILES ${SRC})

# add platform specific sources
if(WIN32)
    set(PLATFORM_SRC
        ${SRCROOT}/Win32/SleepImpl.cpp
        ${SRCROOT}/Win32/SleepImpl.hpp
        ${SRCROOT}/Win32/WindowsHeader.hpp
    )
    source_group("windows" FILES ${PLATFORM_SRC})
else()
    set(PLATFORM_SRC
        ${SRCROOT}/Unix/SleepImpl.cpp
        ${SRCROOT}/Unix/SleepImpl.hpp
    )

    if(ANDROID)
        set(PLATFORM_SRC ${PLATFORM_SRC}
            ${SRCROOT}/Android/Activity.cpp
            ${SRCROOT}/Android/Activity.hpp
            ${SRCROOT}/Android/NativeActivity.cpp
            ${SRCROOT}/Android/ResourceStream.cpp
            ${SRCROOT}/Android/ResourceStream.hpp
            ${SRCROOT}/Android/SuspendAwareClock.cpp
        )
    endif()

    source_group("unix" FILES ${PLATFORM_SRC})
endif()

# define the sfml-system target
add_library(sfml-system STATIC ${SRC} ${PLATFORM_SRC})

target_include_directories(sfml-system SYSTEM BEFORE PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../.. ${CMAKE_CURRENT_SOURCE_DIR}/../../../include)

target_compile_definitions(sfml-system PRIVATE   SFML_STATIC)
target_compile_definitions(sfml-system INTERFACE SFML_STATIC)

if(ANDROID)
    # glad sources
    target_include_directories(sfml-system SYSTEM BEFORE PRIVATE "${PROJECT_SOURCE_DIR}/extlibs/headers/glad/include")
endif()

# setup dependencies
if(UNIX OR APPLE)
    target_link_libraries(sfml-system PRIVATE pthread)
endif()
if(UNIX AND NOT APPLE)
    target_link_libraries(sfml-system PRIVATE rt)
elseif(WIN32)
    target_link_libraries(sfml-system PRIVATE winmm)
elseif(ANDROID)
    target_link_libraries(sfml-system PRIVATE android log)
endif()

target_link_libraries(sfml-system PRIVATE nonstd-lib ghc-filesystem)
