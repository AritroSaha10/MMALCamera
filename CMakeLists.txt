project( MMALCamera )
cmake_minimum_required( VERSION 2.8 )

set( CMAKE_CXX_STANDARD 11 )
set( CMAKE_CXX_STANDARD_REQUIRED 11 )

include_directories( /opt/vc/include/ )

add_executable( ${PROJECT_NAME} main.cpp MMALCamera.cpp )
target_link_libraries( ${PROJECT_NAME} /opt/vc/lib/libmmal_core.so /opt/vc/lib/libmmal_util.so /opt/vc/lib/libmmal.so /opt/vc/lib/libvcos.so )
