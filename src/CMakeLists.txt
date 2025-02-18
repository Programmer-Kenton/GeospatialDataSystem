add_subdirectory(public)
add_subdirectory(utils)
add_subdirectory(server)
add_subdirectory(service)

add_executable(main main.cpp)
target_link_libraries(main public_lib utils_lib server_lib service_lib ${Python3_LIBRARY})
