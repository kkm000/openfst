file(GLOB HEADER_FILES ../../include/fst/extensions/ngram/*.h)
message(STATUS "${HEADER_FILES}")

add_library(fstngram
  bitmap-index.cc 
  ngram-fst.cc 
  nthbit.cc
  ${HEADER_FILES}
)

target_link_libraries(fstngram
  fst
)

set_target_properties(fstngram
  PROPERTIES SOVERSION "7"
)

install(TARGETS fstngram 
	LIBRARY DESTINATION lib
	ARCHIVE DESTINATION lib
	RUNTIME DESTINATION lib
)

add_library(ngram_fst MODULE
  bitmap-index.cc 
  ngram-fst.cc 
  nthbit.cc
)

target_link_libraries(ngram_fst
  fst
)

install(TARGETS ngram_fst
  LIBRARY DESTINATION lib/fst
)

