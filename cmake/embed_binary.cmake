if(NOT DEFINED INPUT_FILE)
    message(FATAL_ERROR "INPUT_FILE is required")
endif()
if(NOT DEFINED OUTPUT_FILE)
    message(FATAL_ERROR "OUTPUT_FILE is required")
endif()
if(NOT DEFINED SYMBOL)
    message(FATAL_ERROR "SYMBOL is required")
endif()

file(READ "${INPUT_FILE}" _hex HEX)
string(LENGTH "${_hex}" _hex_len)
math(EXPR _byte_count "${_hex_len} / 2")
string(REGEX REPLACE "([0-9a-fA-F][0-9a-fA-F])" "0x\\1, " _bytes "${_hex}")
string(REGEX REPLACE "((0x[0-9a-fA-F][0-9a-fA-F], ){16})" "\\1\n" _bytes "${_bytes}")

get_filename_component(_out_dir "${OUTPUT_FILE}" DIRECTORY)
file(MAKE_DIRECTORY "${_out_dir}")
file(WRITE "${OUTPUT_FILE}"
    "#include <cstddef>\n\n"
    "extern const unsigned char ${SYMBOL}[] = {\n"
    "${_bytes}\n"
    "};\n\n"
    "extern const unsigned int ${SYMBOL}_size = ${_byte_count}u;\n")
