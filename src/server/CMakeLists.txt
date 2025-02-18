file(GLOB SERVER_SOURCES *.cpp *.h)
add_library(server_lib ${SERVER_SOURCES})
target_link_directories(server_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})