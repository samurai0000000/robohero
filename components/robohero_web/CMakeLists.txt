idf_component_register(SRCS "robohero_web.c"
                       INCLUDE_DIRS "include"
                       REQUIRES robohero_app robohero_servo robohero_store esp_http_server
                       EMBED_FILES "index.html" "calibrate.html")
