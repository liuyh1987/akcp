#add_executable(concurrency_components_unittest concurrency_components_unittest.cpp)
#target_link_libraries(concurrency_components_unittest gtest akcp boost_system pthread boost_thread)

#add_executable(kcp_session_unittest kcp_session_unittest.cpp)
#target_link_libraries(kcp_session_unittest  gtest akcp boost_system pthread boost_thread)

set(base_SRCS
    concurrency_components_unittest.cpp
    kcp_session_unittest.cpp
    all_unittest.cpp
  )

add_executable(alltest ${base_SRCS})
target_link_libraries(alltest gtest akcp pthread)
