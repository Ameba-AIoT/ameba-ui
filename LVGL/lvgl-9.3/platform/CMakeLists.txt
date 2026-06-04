##########################################################################################
## * This part defines public part of the component

set(public_includes)
set(public_definitions)
set(public_libraries)

ameba_global_include(${public_includes})
ameba_global_define(${public_definitions})
ameba_global_library(${public_libraries})

##########################################################################################
## * This part defines private part of the component

set(private_sources)
set(private_includes)
set(private_definitions)
set(private_compile_options)

#------------------------------#
# Component private part, user config begin

ameba_list_append(private_sources
    lv_port.c
)

ameba_list_append_if(CONFIG_AMEBASMART private_sources
    lv_port_touch.c
)

ameba_list_append(private_includes
    .
    ../lvgl
    ../hal/include/common
    ../../../display
    ../../../input
)

# drm/core/ is already a global include when CONFIG_DISPLAY_DRM=y
# (exported by drm/CMakeLists.txt public_includes)

# Component private part, user config end
#------------------------------#

ameba_add_internal_library(lv_port
    p_SOURCES
        ${private_sources}
    p_INCLUDES
        ${private_includes}
    p_DEFINITIONS
        ${private_definitions}
    p_COMPILE_OPTIONS
        ${private_compile_options}
)
##########################################################################################
