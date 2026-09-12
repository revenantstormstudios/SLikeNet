#
# This file was taken from RakNet 4.082.
# Please see licenses/RakNet license.txt for the underlying license and related copyright.
#
#
# Modified work: Copyright (c) 2016-2019, SLikeSoft UG (haftungsbeschränkt)
#
# This source code was modified by SLikeSoft. Modifications are licensed under the MIT-style
# license found in the license.txt file in the root directory of this source tree.
#

cmake_minimum_required(VERSION 2.6)

# CMake policy settings
if( POLICY CMP0037 )
	cmake_policy(SET CMP0037 NEW) # CMake 3.0 warning: Target names should not be reserved and should match a validity pattern (aka: add_*-command target names)
endif()
if( POLICY CMP0042 )
	cmake_policy(SET CMP0042 NEW) # CMake 3.0 warning: Use @rpath in a target's install name.
endif()

project(SLikeNet)

# version number
set(SLikeNet_VERSION_MAJOR "0")
set(SLikeNet_VERSION_MINOR "2")
set(SLikeNet_VERSION_PATCH "0")
set(SLikeNet_VERSION ${SLikeNet_VERSION_MAJOR}.${SLikeNet_VERSION_MINOR}.${SLikeNet_VERSION_PATCH})
set(SLikeNet_API_VERSION ${SLikeNet_VERSION_MAJOR}.${SLikeNet_VERSION_MINOR})

# explicitly enable @rpath in the install name for any shared library being built (for cmake >=2.8.12 and <3.0 - it's enabled by default for >= 3.0)
# note that for cmake < 2.8.12 we do not use rpath but rather keep the RakNet 4.082 behavior
if( CMAKE_VERSION VERSION_LESS 3.0 )
	if(NOT (CMAKE_VERSION VERSION_LESS 2.8.12))
		# MACOSX_RPATH support was added in cmake 2.8.12
		set(CMAKE_MACOSX_RPATH 1)
	endif()
endif()

IF (WIN32 AND NOT UNIX)
	set (PROGRAMFILESX86 $ENV{PROGRAMFILES})
	string(REPLACE "\\" "/" PROGRAMFILESX86 "${PROGRAMFILESX86}")
ENDIF(WIN32 AND NOT UNIX)

# add package dependencies
# The CryptoManager / FileEncrypter classes under Source/src/crypto are the only part of the
# library that requires OpenSSL; nothing else in Source/ includes their headers. Gate them
# behind an option so the library can be built and tested without OpenSSL available. This
# defaults to TRUE, so the shipped configuration is unchanged.
option(SLIKENET_ENABLE_CRYPTO      "Build the OpenSSL-backed CryptoManager/FileEncrypter classes." TRUE)

if(SLIKENET_ENABLE_CRYPTO)
	find_package(OpenSSL 1.0.0 REQUIRED)
	set(SLIKENET_LIBRARY_LIBS OpenSSL::Crypto OpenSSL::SSL)
else()
	set(SLIKENET_LIBRARY_LIBS "")
endif()

IF (WIN32 AND NOT UNIX)
	set(SLIKENET_LIBRARY_LIBS ${SLIKENET_LIBRARY_LIBS} ws2_32.lib)
ELSE(WIN32 AND NOT UNIX)
	set(SLIKENET_LIBRARY_LIBS ${SLIKENET_LIBRARY_LIBS} pthread)
ENDIF(WIN32 AND NOT UNIX)

# Source/src/SecureHandshake.cpp #includes the libcat .cpp files directly, so with
# LIBCAT_SECURITY enabled the "#pragma comment(lib, big_x64.lib)" in cat/src/math/BigRTL.cpp
# ends up inside SecureHandshake.obj. Every executable that pulls that object out of the
# library therefore needs the prebuilt assembly library on its link path. It ships in the tree,
# but no directory was ever added for it, so such a link failed with LNK1104. The pragma is
# guarded on MSVC plus 64-bit, so match that here.
IF (MSVC AND CMAKE_SIZEOF_VOID_P EQUAL 8)
	link_directories(${SLikeNet_SOURCE_DIR}/DependentExtensions/cat/lib/cat)
ENDIF()

# enable C++11 language support for GCC
include(CheckCXXCompilerFlag)
CHECK_CXX_COMPILER_FLAG("-std=c++11" COMPILER_SUPPORTS_CXX11)
CHECK_CXX_COMPILER_FLAG("-std=c++0x" COMPILER_SUPPORTS_CXX0X)
if(COMPILER_SUPPORTS_CXX11)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
elseif(COMPILER_SUPPORTS_CXX0X)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++0x")
else()
    message(STATUS "The compiler ${CMAKE_CXX_COMPILER} has no C++11 support. Please use a different C++ compiler.")
endif()

# options
IF (WIN32 AND NOT UNIX)
	option(SLIKENET_ENABLE_SAMPLES "Generate sample projects if true."                 TRUE)
ELSE (WIN32 AND NOT UNIX)
	# building samples is disabled atm by default on Unix/Mac, since the sample projects won't compile correctly
	option(SLIKENET_ENABLE_SAMPLES "Generate sample projects if true."                 FALSE)
ENDIF(WIN32 AND NOT UNIX)
option(SLIKENET_ENABLE_DLL         "Generate the DLL / shared object project if true." TRUE)
option(SLIKENET_ENABLE_STATIC      "Generate the static library project if true."      TRUE)
option(SLIKENET_ENABLE_TESTS       "Generate the unit test project if true."           TRUE)
option(SLIKENET_ENABLE_ASAN        "Build with AddressSanitizer (for test/CI builds)."  FALSE)

# AddressSanitizer turns the out-of-bounds accesses the tests probe for into hard failures
# instead of silent corruption. This is applied with add_compile_options rather than by
# appending to CMAKE_CXX_FLAGS, because Lib/LibStatic/CMakeLists.txt overwrites
# CMAKE_CXX_FLAGS wholesale and would otherwise discard the flag for the library itself.
if(SLIKENET_ENABLE_ASAN)
	if(MSVC)
		# /Zi so ASan reports carry file and line information rather than raw addresses;
		# without it MSVC emits C5072 for every translation unit and LNK4302 at link time.
		add_compile_options(/fsanitize=address /Zi)
		if(NOT (CMAKE_VERSION VERSION_LESS 3.13))
			# ASan is incompatible with incremental linking.
			add_link_options(/INCREMENTAL:NO /DEBUG)
		endif()
	else()
		add_compile_options(-fsanitize=address -fno-omit-frame-pointer)
		if(NOT (CMAKE_VERSION VERSION_LESS 3.13))
			add_link_options(-fsanitize=address)
		endif()
	endif()
endif()

set(SLIKENET_HEADER_FILES ${SLikeNet_SOURCE_DIR}/Source)

include(./CmakeIncludes/CmakeMacros.txt)
FIXLINKOPTIONS()
FIXCOMPILEOPTIONS()

add_subdirectory(Lib)

set(SLIKENET_COMMON_LIBS SLikeNetLibStatic)

if(SLIKENET_ENABLE_SAMPLES)
	add_subdirectory(Samples)
endif()

# The tests link against the static library, so they cannot be built without it.
if(SLIKENET_ENABLE_TESTS AND NOT SLIKENET_ENABLE_STATIC)
	message(WARNING "SLIKENET_ENABLE_TESTS requires SLIKENET_ENABLE_STATIC; tests will not be generated.")
elseif(SLIKENET_ENABLE_TESTS)
	enable_testing()
	add_subdirectory(Tests)
endif()
