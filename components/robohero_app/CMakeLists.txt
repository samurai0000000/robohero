idf_component_register(SRCS "robohero_app.c"
                       INCLUDE_DIRS "include"
                       REQUIRES robohero_config robohero_store robohero_motions
                                robohero_servo nvs_flash)
