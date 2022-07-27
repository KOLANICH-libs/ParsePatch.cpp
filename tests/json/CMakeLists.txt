find_package(json_struct)

buildAndPackageLib("json_tests"
	COMPONENT "tests"
	DESCRIPTION "A common library for testing of this lib using the JSON test suite"
	PUBLIC_INCLUDES ${Include_dir} "${pkgcfg_include_fileTestSuite_runner_FileTestSuite_runner}"
	PRIVATE_LIBS "${pkgcfg_lib_fileTestSuite_runner_FileTestSuite_runner}" "lib${PROJECT_NAME}" "json_struct::json_struct" "${pkgcfg_lib_fileTestSuite_runner_FileTestSuite_runner}"
)
