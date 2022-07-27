buildAndPackageLib(${PROJECT_NAME}
	TARGET_NAME_WITH_LIB_PREFIX
	COMPONENT "library"
	DESCRIPTION "${PROJECT_DESCRIPTION}"
	PUBLIC_INCLUDES ${Include_dir}
	PRIVATE_INCLUDES "${expected_include_dirs}"
)
#target_compile_options(libparsepatch PRIVATE "-ferror-limit=100500")
