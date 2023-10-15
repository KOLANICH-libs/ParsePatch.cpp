cmake_minimum_required(VERSION 3.13)

set(CMAKE_USE_RELATIVE_PATHS TRUE)
project("ParsePatch")
set("PROJECT_DESCRIPTION" "A C++ port of https://github.com/mozilla/rust-parsepatch MPL-2.0-licensed patch file parsing library.")
set("PROJECT_HOMEPAGE_URL" "https://codeberg.org/KFmts/ParsePatch.cpp")
set(CPACK_PACKAGE_VENDOR "KOLANICH")
message(STATUS "We have moved from M$ GitHub to ${PROJECT_HOMEPAGE_URL}, read why on https://codeberg.org/KOLANICH/Fuck-GuanTEEnomo . ")

set(CPACK_PACKAGE_VERSION_MAJOR "0")
set(CPACK_PACKAGE_VERSION_MINOR "2")
set(CPACK_PACKAGE_VERSION_PATCH "8")
set(CPACK_PACKAGE_VERSION "${CPACK_PACKAGE_VERSION_MAJOR}.${CPACK_PACKAGE_VERSION_MINOR}.${CPACK_PACKAGE_VERSION_PATCH}")

set(OUR_CMAKE_MODULES_DIR "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
set(OUR_CMAKE_3PARTY_MODULES_DIR "${OUR_CMAKE_MODULES_DIR}/thirdParty")
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${OUR_CMAKE_MODULES_DIR}" "${OUR_CMAKE_MODULES_DIR}/Boilerplate" "${OUR_CMAKE_MODULES_DIR}/DoxygenUtils" "${OUR_CMAKE_3PARTY_MODULES_DIR}")

include(Boilerplate)

set(Include_dir "${CMAKE_CURRENT_SOURCE_DIR}/include")
file(GLOB_RECURSE HDRFILES "${Include_dir}/*.h" "${Include_dir}/*.hpp")

set(LibSource_dir "${CMAKE_CURRENT_SOURCE_DIR}/src")
set(PackagingTemplatesDir "${CMAKE_CURRENT_SOURCE_DIR}/packaging")
set(tests_dir "${CMAKE_CURRENT_SOURCE_DIR}/tests")

set(CPACK_PACKAGE_MAINTAINER "${CPACK_PACKAGE_VENDOR}")
set(CPACK_DEBIAN_PACKAGE_NAME "${CPACK_PACKAGE_NAME}")
set(CPACK_RPM_PACKAGE_NAME "${CPACK_PACKAGE_NAME}")

add_subdirectory("${LibSource_dir}")

option(WITH_TESTS ON "Enable testing")

if(WITH_TESTS)
	add_subdirectory("${tests_dir}")
	enable_testing()
endif()

option(WITH_DOCS "Build docs" OFF)
if(WITH_DOCS)
	include(DoxygenUtils)
	find_package(Doxygen REQUIRED dot)

	load_doxyfile("${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile")
	set(DOXYGEN_PROJECT_BRIEF "${CPACK_PACKAGE_DESCRIPTION}")
	set(DOXYGEN_USE_MDFILE_AS_MAINPAGE "${CPACK_RESOURCE_FILE_README}")
	set(DOXYGEN_EXAMPLE_PATH "${Examples_Dir}")

	doxygen_document_target("docs" "lib${PROJECT_NAME}" "html;man")
endif()

set(CPACK_DEB_COMPONENT_INSTALL ON)
set(CPACK_RPM_COMPONENT_INSTALL ON)
set(CPACK_NSIS_COMPONENT_INSTALL ON)
#set(CPACK_DEBIAN_ENABLE_COMPONENT_DEPENDS ON)
set(CPACK_RPM_ENABLE_COMPONENT_DEPENDS ON)
set(CPACK_DEBIAN_COMPRESSION_TYPE "xz")

include(CPack)
