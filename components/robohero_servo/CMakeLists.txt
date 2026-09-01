idf_component_register(SRCS "robohero_servo.c"
                       INCLUDE_DIRS "include"
                       REQUIRES robohero_config robohero_motions
                                robohero_store pca9685)
