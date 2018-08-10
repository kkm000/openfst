file(GLOB HEADER_FILES ../../include/fst/extensions/compress/*.h)
message(STATUS "${HEADER_FILES}")

 
add_library(fstcompressscript
  compress-script.cc
  ${HEADER_FILES}
 )

target_link_libraries(fstcompressscript 
  fstscript
  fst
  ${ZLIB_LIBRARIES}
)

target_include_directories(fstcompressscript
  PRIVATE ${ZLIB_INCLUDE_DIRS})

set_target_properties(fstcompressscript PROPERTIES
  SOVERSION "${SOVERSION}"
  FOLDER compress
)
install(TARGETS fstcompressscript 
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib
  RUNTIME DESTINATION lib
 )

if(HAVE_BIN)
  add_executable(fstcompress 
    fstcompress.cc
  )
  target_include_directories(fstcompress
    PRIVATE ${ZLIB_INCLUDE_DIRS}
  )
  set_target_properties(fstcompress PROPERTIES
    FOLDER compress/bin
  )
  target_link_libraries(fstcompress 
    fstcompressscript 
    fstscript 
    fst
    ${ZLIB_LIBRARIES}
    ${CMAKE_DL_LIBS}
  )

  add_executable(fstrandmod
    fstrandmod.cc
  )
  set_target_properties(fstrandmod PROPERTIES
    FOLDER compress/bin
  )
  target_include_directories(fstrandmod
    PRIVATE ${ZLIB_INCLUDE_DIRS}
  )
  target_link_libraries(fstrandmod
    fstcompressscript 
    fstscript 
    fst
    ${ZLIB_LIBRARIES}
    ${CMAKE_DL_LIBS}
  )

  install(TARGETS fstcompress fstrandmod 
	        LIBRARY DESTINATION bin
			ARCHIVE DESTINATION bin
            RUNTIME DESTINATION bin
		)
endif(HAVE_BIN)
