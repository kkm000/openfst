function (add_module _name)
    add_library(${ARGV})
    if (TARGET ${_name})
        target_link_libraries(${_name} fst)
    endif()

    install(TARGETS ${_name} LIBRARY DESTINATION lib/fst)
endfunction()


add_module(const8-fst MODULE const8-fst.cc)

add_module(const16-fst MODULE const16-fst.cc)

add_module(const64-fst MODULE const64-fst.cc)

add_library(fstconst 
  const8-fst.cc 
  const16-fst.cc 
  const64-fst.cc)
target_link_libraries(fstconst fst)
set_target_properties(fstconst
  PROPERTIES SOVERSION "7"
)

install(TARGETS fstconst 
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib
  RUNTIME DESTINATION lib
 )
