list(APPEND package_list
     libtool m4 automake)

foreach(package IN LISTS package_list)
    execute_process(COMMAND dpkg-query -W -f='\${Status}\\n' ${package}
                    OUTPUT_VARIABLE PACKAGE_INSTALL_MESSAGE
		            ERROR_VARIABLE PACKAGE_INSTALL_MESSAGE)
    if (NOT ${PACKAGE_INSTALL_MESSAGE} MATCHES "install ok installed")
        execute_process(COMMAND sudo apt-get install -y ${package})
    endif() 
endforeach()
