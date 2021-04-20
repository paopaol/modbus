# include(ExternalProject)

# ExternalProject_Add( fmtlib URL ${CMAKE_CURRENT_SOURCE_DIR}/fmt-6.1.2.tar.gz
# PREFIX fmtlib CMAKE_ARGS -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
# -DCMAKE_INSTALL_PREFIX=<INSTALL_DIR> -DFMT_TEST=OFF
# -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}
# -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}) set_target_properties(fmtlib
# PROPERTIES FOLDER "fmtlib")

include(FetchContent)

# set(FMT_TEST 0) set(BUILD_SHARED_LIBS 1)

FetchContent_Declare(
  fmt
  URL ${CMAKE_CURRENT_SOURCE_DIR}/fmt-6.1.2.tar.gz
  URL_HASH MD5=2914e3ac33595103d6b27c87364b034f)

FetchContent_MakeAvailable(fmt)
