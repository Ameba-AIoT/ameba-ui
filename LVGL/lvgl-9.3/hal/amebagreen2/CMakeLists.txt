##########################################################################################
## * This part defines public part of the component

set(public_includes)
set(public_definitions)
set(public_libraries)

ameba_list_append(public_includes
    ../include/common
)

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
    lv_ameba_jpeg.c
    lv_draw_ppe.c
    lv_ameba_hal.c
)

ameba_list_append(private_compile_options
    -Wno-incompatible-pointer-types
)

ameba_list_append(private_includes
    ../include/amebagreen2
    ../include/common
    ../../lvgl
    ${c_CMPT_FWLIB_DIR}/jpeg_decoder/inc
)

# Component private part, user config end
#------------------------------#

ameba_add_internal_library(lvgl_drivers
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
