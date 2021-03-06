set(LIBRARY_NAME SAMPAsim)

if (${SYSTEMC_FOUND})
include_directories(
  ${SYSTEMC_INCDIR}
)

link_directories(
  ${SYSTEMC_LIBDIR}
)

set(SOURCES
  Monitor.cpp
  Graph.cpp
  Mapper.cpp
  Channel.cpp
  GBT.cpp
  CRU.cpp
  SAMPA.cpp
  DataGenerator.cpp
  Packet.cpp
  Sample.cpp
  RandomGenerator.cpp
  Huffman.cpp
)

string (REGEX REPLACE "\\.cxx" ".obj" OBJECTS "${SOURCES}")

set(DEPENDENCIES
  ${DEPENDENCIES}
  ${SYSTEMC_LIBRARIES}
)

#set(DEPENDENCIES
#  ${DEPENDENCIES}
#   boost_thread boost_system boost_chrono
#)

add_library(
  ${LIBRARY_NAME}
  SHARED
  ${SOURCES}
  )

target_link_libraries( ${LIBRARY_NAME}
  ${DEPENDENCIES}
)

install(TARGETS ${LIBRARY_NAME} DESTINATION lib)

Set(Exe_Names
  runSAMPA
)

set(Exe_Source
  main.cpp
)

list(LENGTH Exe_Names _length)
math(EXPR _length ${_length}-1)

ForEach(_file RANGE 0 ${_length})
  list(GET Exe_Names ${_file} _name)
  list(GET Exe_Source ${_file} _src)
  set(EXE_NAME ${_name})
  set(SRCS ${_src})
  set(DEPENDENCIES ${LIBRARY_NAME} dl)
  add_executable(${EXE_NAME} ${SRCS})
  target_link_libraries(${EXE_NAME} ${DEPENDENCIES})
  install(TARGETS ${EXE_NAME} DESTINATION bin)
EndForEach(_file RANGE 0 ${_length})
else()
message(WARNING "SystemC required to build module '${LIBRARY_NAME}', skipping ...")
endif (${SYSTEMC_FOUND})
