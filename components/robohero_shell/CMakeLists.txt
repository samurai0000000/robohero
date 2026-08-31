idf_component_register(SRCS "robohero_shell.c"
                       INCLUDE_DIRS "include" "${CMAKE_BINARY_DIR}" "${CMAKE_BINARY_DIR}/include"
                       REQUIRES robohero_app robohero_store robohero_servo robohero_web)
