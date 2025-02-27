file(GLOB LOG_SOURCES *.cpp *.h)
add_library(log_lib ${LOG_SOURCES})
target_link_directories(log_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})