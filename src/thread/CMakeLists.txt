file(GLOB THREAD_SOURCES *.cpp *.h)
add_library(thread_lib ${THREAD_SOURCES})
target_link_directories(thread_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})