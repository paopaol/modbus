include(ExternalProject)

ExternalProject_Add(googletest
		URL             ${CMAKE_CURRENT_SOURCE_DIR}/googletest-release-1.8.1.tar.gz
		PREFIX googletest
		CMAKE_ARGS
						-Dgtest_force_shared_crt=ON
						-DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>
)
set_target_properties(googletest PROPERTIES FOLDER "googletest")
