set(CHKSUM_PREFIX1 "\"")
set(CHKSUM_PREFIX2 "\": \"")
set(CHKSUM_SUFFIX "\"")

if(NOT DEFINED EXECUTABLE)
    set(EXECUTABLE CACHE STRING "EXECUTABLE")
endif()

if(NOT DEFINED OUT_DIR)
    set(OUT_DIR CACHE STRING "OUT_DIR")
endif()
if(NOT DEFINED XML_FILE)
    set(XML_FILE CACHE STRING "XML_FILE")
endif()
if(NOT DEFINED HOME)
	set(HOME CACHE STRING "HOME")
endif()

set(EXEC_PATH ${OUT_DIR}/${EXECUTABLE})

message("Path: ${EXEC_PATH}")

if(EXISTS ${EXEC_PATH})
	file(SHA1 ${EXEC_PATH} exe_sha1)
	set(sha1_chksum "${CHKSUM_PREFIX1}sha1${CHKSUM_PREFIX2}${exe_sha1}${CHKSUM_SUFFIX}")

	file(SHA512 ${EXEC_PATH} exe_sha512)
	set(sha512_chksum "${CHKSUM_PREFIX1}sha512${CHKSUM_PREFIX2}${exe_sha512}${CHKSUM_SUFFIX}")

	#Skip first hash to avoid fencepost issue for foreach loop
	set(hashes ${sha512_chksum})

	set(executable_details "\t\t\"${CMAKE_SYSTEM_NAME}\": {\n\t\t\t\"file_name\": \"${EXECUTABLE}\",\n")
	set(executable_details "${executable_details}\t\t\t${sha1_chksum}")

	foreach(hash IN LISTS hashes)
		set(executable_details "${executable_details},\n\t\t\t${hash}")
	endforeach()
	set(executable_details "${executable_details}\n\t\t}")
else()
		message(WARNING "The excutable does not exist! Skipping hashes.")
endif()

file(READ "${HOME}/resources/plugin.json" json)
STRING(REPLACE "\t\t\"!OS\": {}" "${executable_details}" json_out ${json})
STRING(REPLACE "!PLUGIN_NAME" "polyslack" json_out ${json_out})
file(WRITE "${OUT_DIR}/${JSON_FILE}" "${json_out}")
