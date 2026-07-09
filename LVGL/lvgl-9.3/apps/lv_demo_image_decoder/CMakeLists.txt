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
    app_example.c
    ../../../../../ui/frameworks/tests/image_decoder/test_visual_decoder.c
)

ameba_list_append(private_includes
    ../../platform
    ../../../../../ui/frameworks/interfaces
    ../../../../../ui/frameworks/tests/image_decoder/
    ../../../../../ui/frameworks/tests/image_decoder/assets
)

# Mirror the HAS_* definitions used by the graphics library so that
# #ifdef HAS_JPEG / HAS_PNG / HAS_QOI guards in the demo code work.
ameba_list_append_if(CONFIG_LIBJPEG_TURBO_ENABLE  private_definitions HAS_JPEG)
ameba_list_append_if(CONFIG_LIBPNG_ENABLE         private_definitions HAS_PNG)
ameba_list_append_if(CONFIG_QOI_ENABLE            private_definitions HAS_QOI)
ameba_list_append_if(CONFIG_IMAGE_DECODER_RLE     private_definitions HAS_RLE)

ameba_list_append(private_compile_options
    -Wno-unused-parameter
)

# Component private part, user end
#------------------------------#

ameba_add_internal_library(lv_demo_image_decoder
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
