file(GLOB SERVICE_SOURCES *.cpp *.h)
file(GLOB UTILS_SOURCES ../utils/*.cpp ../utils/*.h)
add_library(service_lib ${SERVICE_SOURCES} ${UTILS_SOURCES})
target_link_directories(service_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})