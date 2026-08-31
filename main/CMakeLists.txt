idf_component_register(SRCS "app_main.c"
                       INCLUDE_DIRS "."
                       REQUIRES robohero_app robohero_web robohero_shell
                                robohero_config nvs_flash)
