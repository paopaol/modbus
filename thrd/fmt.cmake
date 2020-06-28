include(ExternalProject)

ExternalProject_Add(fmtlib
		URL             ${CMAKE_CURRENT_SOURCE_DIR}/fmt-6.1.2.tar.gz
		PREFIX fmtlib
		CMAKE_ARGS
		    -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
		    -DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>
		    -DFMT_TEST=OFF
		    -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}
		    -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}
)
set_target_properties(fmtlib PROPERTIES FOLDER "fmtlib")
