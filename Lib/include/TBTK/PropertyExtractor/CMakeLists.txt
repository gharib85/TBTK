#ADD_SUBDIRECTORY(ArnoldiIterator/TBTK/PropertyExtractor/)

INSTALL(
	DIRECTORY .
	DESTINATION include/TBTK/PropertyExtractor
	FILES_MATCHING PATTERN *.h
	PATTERN ArnoldiIterator.h EXCLUDE
	PATTERN ArnoldiIterator* EXCLUDE
	PATTERN MatsubaraSusceptibility.h EXCLUDE
	PATTERN SelfEnergy2.h EXCLUDE
)

IF(${COMPILE_ARNOLDI_ITERATOR})
	INSTALL(
		DIRECTORY .
		DESTINATION include/TBTK/PropertyExtractor
		FILES_MATCHING PATTERN ArnoldiIterator.h
	)
ENDIF(${COMPILE_ARNOLDI_ITERATOR})
IF(${COMPILE_FOURIER_TRANSFORM})
	INSTALL(
		DIRECTORY .
		DESTINATION include/TBTK/PropertyExtractor
		FILES_MATCHING PATTERN MatsubaraSusceptibility.h
	)
	INSTALL(
		DIRECTORY .
		DESTINATION include/TBTK/PropertyExtractor
		FILES_MATCHING PATTERN SelfEnergy2.h
	)
ENDIF(${COMPILE_FOURIER_TRANSFORM})
