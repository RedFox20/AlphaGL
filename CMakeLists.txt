cmake_minimum_required(VERSION 3.8)
project(AlphaGL)

include(mama.cmake)
message("MAMA_INCLUDES=${MAMA_INCLUDES}")
include_directories(
    ${MAMA_INCLUDES}
    "."
)

option(AGL_TESTS "Enable AlphaGL Tests executable" ON)

if(WIN32)
    set(RUNTIME opengl32.lib)
elseif(LINUX)
    set(RUNTIME GL X11)
endif()

file(GLOB AGL_SOURCES AGL/*.cpp AGL/*.h AGL/*.c)
if(WIN32)
    set(AGL_SOURCES ${AGL_SOURCES} AGL/AGL.natvis)
endif()

add_library(AGL ${AGL_SOURCES})
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${AGL_SOURCES})

target_link_libraries(AGL PRIVATE ${MAMA_LIBS} PRIVATE ${RUNTIME})
set_property(TARGET AGL PROPERTY CXX_STANDARD 17)
set_property(TARGET AGL PROPERTY CXX_STANDARD_REQUIRED)

install(TARGETS AGL DESTINATION ${CMAKE_CURRENT_SOURCE_DIR}/lib)



file(GLOB AGL_TESTS_SOURCES tests/*.cpp tests/*.h)
add_executable(AGLTests ${AGL_TESTS_SOURCES})
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${AGL_TESTS_SOURCES})

target_link_libraries(AGLTests AGL ${MAMA_LIBS} ${RUNTIME})
set_property(TARGET AGLTests PROPERTY CXX_STANDARD 17)
set_property(TARGET AGLTests PROPERTY CXX_STANDARD_REQUIRED)

install(TARGETS AGLTests DESTINATION ${CMAKE_CURRENT_SOURCE_DIR}/bin)

