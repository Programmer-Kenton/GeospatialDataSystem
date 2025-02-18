file(GLOB UTILS_SOURCES *.cpp *.h)
add_library(utils_lib ${UTILS_SOURCES})
target_include_directories(utils_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
