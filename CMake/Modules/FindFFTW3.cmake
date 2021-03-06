FIND_PATH(
	FFTW3_INCLUDES
	NAMES fftw3.h
	PATHS ${FFTW3_INCLUDE_PATH}
)

FIND_LIBRARY(
	FFTW3_LIBRARIES
	NAMES fftw3
	PATHS ${FFTW3_LIBRARY_PATH}
	PATH_SUFFIXES lib lib32 lib64
)

INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(
	FFTW3
	DEFAULT_MSG
	FFTW3_LIBRARIES
	FFTW3_INCLUDES
)

#MARK_AS_ADVANCED(FFTW_LIBRARIES FFTW_INCLUDES)
