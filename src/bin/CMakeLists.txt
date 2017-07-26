function (add_executable2 _name)
    add_executable(${ARGV})
    if (TARGET ${_name})
        target_link_libraries(${_name} fstscript fst ${CMAKE_DL_LIBS})
    endif()

    install(TARGETS ${_name} RUNTIME DESTINATION bin)
endfunction()

include_directories(../include ../script/)

add_executable2(fstarcsort fstarcsort.cc)

add_executable2(fstclosure fstclosure.cc)

add_executable2(fstcompile  fstcompile.cc)

add_executable2(fstcompose  fstcompose.cc)

add_executable2(fstconcat  fstconcat.cc)

add_executable2(fstconnect  fstconnect.cc)

add_executable2(fstconvert  fstconvert.cc)

add_executable2(fstdeterminize  fstdeterminize.cc)

add_executable2(fstdifference  fstdifference.cc)

add_executable2(fstdisambiguate  fstdisambiguate.cc)

add_executable2(fstdraw  fstdraw.cc)

add_executable2(fstencode  fstencode.cc)

add_executable2(fstepsnormalize  fstepsnormalize.cc)

add_executable2(fstequal  fstequal.cc)

add_executable2(fstequivalent  fstequivalent.cc)

add_executable2(fstinfo  fstinfo.cc)

add_executable2(fstintersect  fstintersect.cc)

add_executable2(fstinvert  fstinvert.cc)

add_executable2(fstisomorphic  fstisomorphic.cc)

add_executable2(fstmap  fstmap.cc)

add_executable2(fstminimize  fstminimize.cc)

add_executable2(fstprint  fstprint.cc)

add_executable2(fstproject  fstproject.cc)

add_executable2(fstprune  fstprune.cc)

add_executable2(fstpush  fstpush.cc)

add_executable2(fstrandgen  fstrandgen.cc)

add_executable2(fstrelabel  fstrelabel.cc)

add_executable2(fstreplace  fstreplace.cc)

add_executable2(fstreverse  fstreverse.cc)

add_executable2(fstreweight  fstreweight.cc)

add_executable2(fstrmepsilon  fstrmepsilon.cc)

add_executable2(fstshortestdistance  fstshortestdistance.cc)

add_executable2(fstshortestpath  fstshortestpath.cc)

add_executable2(fstsymbols  fstsymbols.cc)

add_executable2(fstsynchronize  fstsynchronize.cc)

add_executable2(fsttopsort  fsttopsort.cc)

add_executable2(fstunion  fstunion.cc)

