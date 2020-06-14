include(ExternalProject)

ExternalProject_Add(fmtlib
		URL             ${CMAKE_CURRENT_SOURCE_DIR}/fmt-6.1.2.tar.gz
		PREFIX fmtlib
		CMAKE_ARGS
						-DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>
						-DFMT_TEST=OFF
)
set_target_properties(fmtlib PROPERTIES FOLDER "fmtlib")
