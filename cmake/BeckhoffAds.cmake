# Wrap Beckhoff/AdsLib as an INTERFACE target for this project.
# Upstream CMake builds target "ads"; we expose it as the requested name (e.g. adslib).

function(beckhoff_add_adslib target_name ads_root)
	if(NOT IS_DIRECTORY "${ads_root}/AdsLib")
		message(FATAL_ERROR
			"Beckhoff ADS not found at '${ads_root}' (expected AdsLib/). "
			"Clone offline: git clone https://github.com/Beckhoff/ADS.git third_party/Beckhoff.ADS")
	endif()

	find_package(Threads REQUIRED)

	add_subdirectory("${ads_root}/AdsLib" "${CMAKE_BINARY_DIR}/beckhoff-ads-lib"
		EXCLUDE_FROM_ALL)

	# Parent project enables AUTOMOC globally; AdsLib is plain C++ (no Qt).
	set_target_properties(ads PROPERTIES AUTOMOC OFF AUTOUIC OFF AUTORCC OFF)

	if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
		target_compile_definitions(ads PRIVATE _GNU_SOURCE)
	endif()
	target_compile_definitions(ads PRIVATE CONFIG_DEFAULT_LOGLEVEL=1)

	add_library(${target_name} INTERFACE)
	target_link_libraries(${target_name} INTERFACE ads)
endfunction()
