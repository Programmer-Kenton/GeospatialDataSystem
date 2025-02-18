file(GLOB PUBLIC_SOURCES *.cpp *.h)
add_library(public_lib ${PUBLIC_SOURCES})
target_link_directories(public_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})