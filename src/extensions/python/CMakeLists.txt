file(GLOB HEADER_FILES ../../include/fst/extensions/python/*.h)
message(STATUS "${HEADER_FILES}")

add_library(pywrapfst pywrapfst.cc ${HEADER_FILES})
target_link_libraries(pywrapfst ${PYTHON_LIBRARIES} fstfarscript fstfar fstscript fst)
target_include_directories(pywrapfst PRIVATE ${PYTHON_INCLUDE_DIRS})
set_target_properties(pywrapfst PROPERTIES 
  FOLDER python/modules
  WINDOWS_EXPORT_ALL_SYMBOLS true
)
install(TARGETS pywrapfst 
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib
  RUNTIME DESTINATION lib
)
