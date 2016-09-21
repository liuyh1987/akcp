set(base_SRCS
	kcp.cpp  
	session.cpp
	client.cpp
	server.cpp
	../3rdparty/kcp/ikcp.cpp
  )

add_library(akcp ${base_SRCS})
#target_link_libraries(akcp boost_system boost_thread)

add_subdirectory(tests)
