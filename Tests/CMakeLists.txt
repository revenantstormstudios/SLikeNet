#
#  Copyright (c) 2026, SLikeNet contributors
#
#  This source code is  licensed under the MIT-style license found in the license.txt
#  file in the root directory of this source tree.
#

cmake_minimum_required(VERSION 3.10)

project(SLikeNetTests)

FILE(GLOB TEST_CPP_SRCS ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)
FILE(GLOB TEST_HEADER_SRCS ${CMAKE_CURRENT_SOURCE_DIR}/*.h)

add_executable(SLikeNetTests ${TEST_CPP_SRCS} ${TEST_HEADER_SRCS})

# Lib/CMakeLists.txt sets SLIKENET_INTERNAL_INCLUDE_DIRS in its own directory scope, which is
# not visible here, so repeat the include paths rather than depending on that variable.
target_include_directories(SLikeNetTests PRIVATE
	${SLikeNet_SOURCE_DIR}/Source/include
	${SLikeNet_SOURCE_DIR}/DependentExtensions
)

# Match the definitions the library itself is built with (see Lib/LibStatic/CMakeLists.txt).
target_compile_definitions(SLikeNetTests PRIVATE _RETAIL)
IF(WIN32 AND NOT UNIX)
	target_compile_definitions(SLikeNetTests PRIVATE WIN32 _CRT_NONSTDC_NO_DEPRECATE _CRT_SECURE_NO_DEPRECATE)
ENDIF(WIN32 AND NOT UNIX)

set_target_properties(SLikeNetTests PROPERTIES
	CXX_STANDARD 17
	CXX_STANDARD_REQUIRED ON
)

target_link_libraries(SLikeNetTests ${SLIKENET_COMMON_LIBS} ${SLIKENET_LIBRARY_LIBS})

# MSVC links the ASan runtime dynamically, and the DLL lives beside cl.exe rather than on the
# default PATH, so a bare test run fails with STATUS_DLL_NOT_FOUND (0xc0000135). Copy it next
# to the executable so the tests are runnable without a developer command prompt.
if(SLIKENET_ENABLE_ASAN AND MSVC)
	get_filename_component(SLIKENET_MSVC_BIN_DIR "${CMAKE_CXX_COMPILER}" DIRECTORY)
	set(SLIKENET_ASAN_RUNTIME "${SLIKENET_MSVC_BIN_DIR}/clang_rt.asan_dynamic-x86_64.dll")
	if(EXISTS "${SLIKENET_ASAN_RUNTIME}")
		add_custom_command(TARGET SLikeNetTests POST_BUILD
			COMMAND ${CMAKE_COMMAND} -E copy_if_different
				"${SLIKENET_ASAN_RUNTIME}" "$<TARGET_FILE_DIR:SLikeNetTests>"
		)
	else()
		message(WARNING "AddressSanitizer runtime not found at ${SLIKENET_ASAN_RUNTIME}; "
		                "run the tests from a developer command prompt instead.")
	endif()
endif()

add_test(NAME SLikeNetTests COMMAND SLikeNetTests)
