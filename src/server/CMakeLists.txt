file(GLOB SERVER_SOURCES *.cpp *.h)
file(GLOB UTILS_SOURCES ../utils/*.cpp ../utils/*.h)
add_library(server_lib ${SERVER_SOURCES} ${UTILS_SOURCES})
target_link_directories(server_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})