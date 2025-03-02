add_executable(testBuildRTree testBuildRTree.cpp)

target_link_libraries(testBuildRTree public_lib utils_lib server_lib service_lib log_lib pthread thread_lib ${Python3_LIBRARY})

add_executable(testPython testPython.cpp)

target_link_libraries(testPython public_lib utils_lib server_lib service_lib log_lib pthread thread_lib ${Python3_LIBRARY})

add_executable(testInsertRTree testInsertRTree.cpp)

target_link_libraries(testInsertRTree public_lib utils_lib server_lib service_lib log_lib pthread thread_lib ${Python3_LIBRARY})

