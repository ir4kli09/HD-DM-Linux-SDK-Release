
function(write_version_header_file FILE_PATH EYS3D_VERSION)
  
    if(${EYS3D_VERSION} STREQUAL "")
        execute_process(COMMAND git rev-parse --short HEAD
                        OUTPUT_VARIABLE VER
                        OUTPUT_STRIP_TRAILING_WHITESPACE)  
    else()
        set(VER ${${EYS3D_VERSION}})
    endif()
    
    set(VERSION_CONTENT 
"#ifndef LIB_ETRONDI_VERSION_H
#define LIB_ETRONDI_VERSION_H
#define ETRONDI_VERSION \"${VER}\"
#endif"
    )

    file(WRITE ${FILE_PATH} ${VERSION_CONTENT})

    set(${EYS3D_VERSION} ${VER} PARENT_SCOPE)

endfunction()