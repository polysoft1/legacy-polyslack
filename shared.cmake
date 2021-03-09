project(PolySlack)

include_directories(${POLYCHAT})
set(executable_filename ${CMAKE_SHARED_LIBRARY_PREFIX}PolySlack${CMAKE_SHARED_LIBRARY_SUFFIX})
set(json_file ${OUTPUT_DIR}/plugin.json)

ADD_LIBRARY(PolySlack SHARED ${src})

get_target_property(DYN_INCLUDE_DIRS PolySlack INCLUDE_DIRECTORIES)
list(APPEND DYN_INCLUDE_DIRS ${PROJECT_SOURCE_DIR})
list(APPEND DYN_INCLUDE_DIRS ${PROJECT_BINARY_DIR})
#if(DEFINED POLYCHAT)
	#target_include_directories(Polymost PUBLIC ${POLYCHAT_INCLUDE})
	#list(APPEND DYN_INCLUDE_DIRS ${POLYCHAT})
	#message("Including ${POLYCHAT}")
	#target_link_libraries(PolySlack ${POLYCHAT}/target/${CMAKE_SHARED_LIBRARY_PREFIX}PolyChat${CMAKE_SHARED_LIBRARY_SUFFIX})
#endif()

find_package(nlohmann_json CONFIG REQUIRED)
target_link_libraries(PolySlack PRIVATE nlohmann_json nlohmann_json::nlohmann_json)

set_target_properties(PolySlack PROPERTIES INCLUDE_DIRECTORIES "${DYN_INCLUDE_DIRS}")

add_custom_target(json_creation)
add_dependencies(json_creation PolySlack)
add_custom_command(TARGET json_creation
	POST_BUILD
	COMMAND cmake -DHOME="${CMAKE_HOME_DIRECTORY}" -DEXECUTABLE="${executable_filename}" -DOUT_DIR="${OUTPUT_DIR}" -DJSON_FILE="plugin.json" -DCMAKE_SYSTEM_NAME="${CMAKE_SYSTEM_NAME}" -P ${CMAKE_HOME_DIRECTORY}/jsoncreate.cmake
) 

add_custom_target(create_zip
	ALL COMMAND ${CMAKE_COMMAND} -E tar "cfv" "PolySlack.zip" --format=zip "${executable_filename}" "${json_file}"
	WORKING_DIRECTORY ${OUTPUT_DIR}
)
add_dependencies(create_zip json_creation PolySlack)
