# A configurable script that unarchives TARGET and all LIBRARIES
# and then merges them back into a single TARGET using the proper ar tool

# Setup
unset (AROBJS)
string(REPLACE " " ";" LIBRARIES ${LIBRARIES})
message(STATUS ${TARGETLOC})
message(STATUS "${LIBRARIES}")
get_filename_component (TARGETNAME ${TARGETLOC} NAME)
set (ARDIR ${CMAKE_CURRENT_BINARY_DIR}/PosixMergeStaticLibraries-${TARGETNAME})
make_directory (${ARDIR})

# Extract every dependent library into a series of object files
foreach (LIB ${LIBRARIES})
	get_filename_component (LIBNAME ${LIB} NAME)
	file (MAKE_DIRECTORY ${ARDIR}/${LIBNAME})
	execute_process (
		COMMAND ${CMAKE_AR} x ${LIB} WORKING_DIRECTORY ${ARDIR}/${LIBNAME}
	)
	file (GLOB_RECURSE OBJS "${ARDIR}/${LIBNAME}/*")
	set (AROBJS ${AROBJS} ${OBJS})
endforeach (LIB)

# Merge all extracted object files into the existing target archive
execute_process (
	COMMAND ${CMAKE_AR} r ${TARGETLOC} ${AROBJS} WORKING_DIRECTORY ${ARDIR}
)

# Ranlib the new archive library
execute_process (
	COMMAND ${CMAKE_RANLIB} ${TARGETLOC} WORKING_DIRECTORY ${ARDIR}
)

# Delete all leftover object files
file (REMOVE_RECURSE ${ARDIR})
