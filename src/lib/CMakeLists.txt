FILE(GLOB HEADER_FILES ../include/fst/*.h)


add_library(fst
  compat.cc
  flags.cc
  fst.cc
  properties.cc
  symbol-table.cc
  util.cc
  symbol-table-ops.cc
  mapped-file.cc
  weight.cc
  ${HEADER_FILES}
)
set_target_properties(fst PROPERTIES
  SOVERSION "6"
)
install(TARGETS fst
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib
  RUNTIME DESTINATION lib)
