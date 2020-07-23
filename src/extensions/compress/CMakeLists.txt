file(GLOB HEADER_FILES ../../include/fst/extensions/compress/*.h)
message(STATUS "${HEADER_FILES}")

add_library(fstcompressscript
  compressscript.cc
  ${HEADER_FILES}
 )

target_link_libraries(fstcompressscript
  fstscript
  fst
  ${ZLIBS}
)
set_target_properties(fstcompressscript PROPERTIES
  SOVERSION "${SOVERSION}"
)
install(TARGETS fstcompressscript
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib
  RUNTIME DESTINATION lib
 )

if(HAVE_BIN)
  add_executable(fstcompress
    fstcompress-main.cc fstcompress.cc)

  target_link_libraries(fstcompress
    fstcompressscript
    fstscript
    fst
    ${ZLIBS}
    ${CMAKE_DL_LIBS}
   )

  install(TARGETS fstcompress
    LIBRARY DESTINATION bin
    ARCHIVE DESTINATION bin
    RUNTIME DESTINATION bin
   )
endif(HAVE_BIN)
