include(ExternalProject)

ExternalProject_Add(
  googletest
  URL ${CMAKE_CURRENT_SOURCE_DIR}/googletest-release-1.8.1.tar.gz
  PREFIX googletest
  CMAKE_ARGS -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
             -Dgtest_force_shared_crt=ON
             -DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>
             -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}
             -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER})
set_target_properties(googletest PROPERTIES FOLDER "googletest")
