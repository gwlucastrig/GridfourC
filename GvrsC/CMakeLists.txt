﻿# CMakeList.txt : Top-level CMake project file for GvrsC, do global configuration
# and include sub-projects here.
#
cmake_minimum_required (VERSION 3.8)

project ("GvrsC" C )

find_package(ZLIB)
 
add_library(${PROJECT_NAME} STATIC)

if( CMAKE_COMPILER_IS_GNUCC )
	message(STATUS "Detected GNU CC compiler")
	target_compile_options(${PROJECT_NAME} PRIVATE -Wall)
endif()


if(ZLIB_FOUND)
	target_compile_definitions(${PROJECT_NAME} PRIVATE GVRS_ZLIB=1 )
	include_directories( ${ZLIB_INCLUDE_DIRS} )
endif()


target_sources(${PROJECT_NAME} PRIVATE
	src/Gvrs.c
	src/GvrsBitInput.c
	src/GvrsBitOutput.c
	src/GvrsBspline.c
	src/GvrsBuilder.c
	src/GvrsChecksum.c
	src/GvrsCodecHuffman.c
	src/GvrsCrossPlatform.c
	src/GvrsElement.c
	src/GvrsFileSpaceManager.c
	src/GvrsInterpolation.c
	src/GvrsM32.c
	src/GvrsMetadata.c
	src/GvrsPredictor.c
	src/GvrsPrimaryIo.c
	src/GvrsRecord.c
	src/GvrsSummarize.c
	src/GvrsTileCache.c
	src/GvrsTileDirectory.c
	)


# The following C files depend on the deflate library (zlib) being available
# on the system.  Because target_sources is cummulative, we can add them
# to the library conditionally.
if(ZLIB_FOUND)
	target_sources(${PROJECT_NAME} PRIVATE
		src/GvrsCodecDeflate.c
		src/GvrsCodecFloat.c
		src/GvrsCodecLsop.c
		)
endif()




target_include_directories(${PROJECT_NAME}
	PRIVATE
		# where the library itself will look for its internal headers
		${CMAKE_CURRENT_SOURCE_DIR}/src
	PUBLIC
		# where top-level project will look for the library's public headers
		$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
		# where external projects will look for the library's public headers
		$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)



# note that it is not CMAKE_INSTALL_PREFIX we are checking here
if(DEFINED CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    message(
        STATUS
        "CMAKE_INSTALL_PREFIX is not set\n"
        "Default value: ${CMAKE_INSTALL_PREFIX}\n"
        "Will set it to ${CMAKE_SOURCE_DIR}/install"
    )
    set(CMAKE_INSTALL_PREFIX
        "${CMAKE_SOURCE_DIR}/install"
        CACHE PATH "Where the library will be installed to" FORCE
    )
else()
    message(
        STATUS
        "CMAKE_INSTALL_PREFIX was already set\n"
        "Current value: ${CMAKE_INSTALL_PREFIX}"
    )
endif()

# without it public headers won't get installed
set(public_headers
	include/Gvrs.h
	include/GvrsBuilder.h
	include/GvrsCodec.h
	include/GvrsCrossPlatform.h
	include/GvrsError.h
	include/GvrsFramework.h
	include/GvrsInternal.h
	include/GvrsInterpolation.h
	include/GvrsMetadata.h
	include/GvrsPrimaryIo.h
	include/GvrsPrimaryTypes.h
	
	
)
# note that ${public_headers} has to be in quotes
set_target_properties(${PROJECT_NAME} PROPERTIES PUBLIC_HEADER "${public_headers}")

set_target_properties(${PROJECT_NAME} PROPERTIES DEBUG_POSTFIX "d")


include(GNUInstallDirs)

# install the target and create export-set
install(TARGETS ${PROJECT_NAME} 
    EXPORT "${PROJECT_NAME}Targets"
    # these get default values from GNUInstallDirs, no need to set them
    #RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} # bin
    #LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} # lib
    #ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR} # lib
    # except for public headers, as we want them to be inside a library folder
    PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/${PROJECT_NAME} # include/SomeLibrary
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR} # include
)



 
