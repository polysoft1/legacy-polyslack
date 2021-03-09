project(PolySlackStatic)

include_directories(${POLYCHAT})
set(executable_filename ${CMAKE_SHARED_LIBRARY_PREFIX}PolySlackStatic${CMAKE_SHARED_LIBRARY_SUFFIX})
set(xml_file ${OUTPUT_DIR}/plugin.xml)

ADD_LIBRARY(PolySlackStatic STATIC ${src})
set_target_properties(PolySlackStatic PROPERTIES LINKER_LANGUAGE CXX)

find_package(nlohmann_json CONFIG REQUIRED)
target_link_libraries(PolySlackStatic PRIVATE nlohmann_json nlohmann_json::nlohmann_json)

get_target_property(STATIC_INCLUDE_DIRS PolySlackStatic INCLUDE_DIRECTORIES)
list(APPEND STATIC_INCLUDE_DIRS ${PROJECT_SOURCE_DIR})
list(APPEND STATIC_INCLUDE_DIRS ${PROJECT_BINARY_DIR})
#if(DEFINED POLYCHAT)
	#target_include_directories(PolySlackStatic PUBLIC ${POLYCHAT_INCLUDE})
	#list(APPEND STATIC_INCLUDE_DIRS ${POLYCHAT})
	#message("Including ${POLYCHAT}")
	#target_link_libraries(PolySlackStatic ${POLYCHAT}/target/${CMAKE_SHARED_LIBRARY_PREFIX}PolyChat${CMAKE_SHARED_LIBRARY_SUFFIX})
#endif()
set_target_properties(PolySlackStatic PROPERTIES INCLUDE_DIRECTORIES "${STATIC_INCLUDE_DIRS}")