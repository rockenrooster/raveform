# Script mode: cmake -DIN=<text file> -DOUT=<header> -P EmbedText.cmake
# Writes the file verbatim as a C++ raw string literal, so source data stays the single source of truth.
if(NOT IN OR NOT OUT)
  message(FATAL_ERROR "EmbedText.cmake needs -DIN=<file> -DOUT=<file>")
endif()
file(READ "${IN}" content)
string(FIND "${content}" ")RFTEXT\"" clash)
if(NOT clash EQUAL -1)
  message(FATAL_ERROR "${IN} contains the raw-string delimiter )RFTEXT\"")
endif()
file(WRITE "${OUT}" "// Generated from ${IN} - do not edit.\nR\"RFTEXT(${content})RFTEXT\"\n")
