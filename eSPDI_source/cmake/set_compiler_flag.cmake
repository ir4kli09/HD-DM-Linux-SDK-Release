include(${CMAKE_CURRENT_LIST_DIR}/setup_eys3d_root.cmake)

# set flags
if(UNIX)	
	set(CMAKE_POSITION_INDEPENDENT_CODE ON)	
	set(CMAKE_CXX_STANDARD 14)
	add_compile_options(-Wall)	
	if($ARCH STREQUAL "x86_64")    
		add_compile_options(-m64)		
	elseif($ARCH STREQUAL "x86")
		add_compile_options(-m32)				
	endif()			
endif()
