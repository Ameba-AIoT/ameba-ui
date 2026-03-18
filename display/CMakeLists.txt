##########################################################################################
## * This part defines public part of the component
## * Public part will be used as global build configures for all component

set(public_includes)                #public include directories, NOTE: relative path is OK
set(public_definitions)             #public definitions
set(public_libraries)               #public libraries(files), NOTE: linked with whole-archive options

#----------------------------------------#
# Component public part, user config begin

# You may use if-else condition to set or update predefined variable above

# Component public part, user config end
#----------------------------------------#

#WARNING: Fixed section, DO NOT change!
ameba_global_include(${public_includes})
ameba_global_define(${public_definitions})
ameba_global_library(${public_libraries}) #default: whole-archived

##########################################################################################
## * This part defines private part of the component
## * Private part is used to build target of current component
## * NOTE: The build API guarantees the global build configures(mentioned above)
## *       applied to the target automatically. So if any configure was already added
## *       to public above, it's unnecessary to add again below.

#NOTE: User defined section, add your private build configures here
# You may use if-else condition to set these predefined variable
# They are only for ameba_add_internal_library/ameba_add_external_app_library/ameba_add_external_soc_library
set(private_sources)                 #private source files, NOTE: relative path is OK
set(private_includes)                #private include directories, NOTE: relative path is OK
set(private_definitions)             #private definitions
set(private_compile_options)         #private compile_options

#------------------------------#
# Component private part, user config begin

ameba_list_append(private_sources
    display_mode_setting.c
    panel_pin_config.c
    panel_manager.c
    panels/panel_st7701s.c
    panels/panel_st7701s_rgb565.c
    panels/panel_st7701p_rgb.c
    panels/panel_hj3508_12.c
    panels/panel_b1620a.c
    panels/panel_t1720a.c
    panels/panel_jd9165ba.c
    panels/panel_spi_init.c
    controller/display_controller.c
    controller/rgb/lcdc_rgb.c
    controller/mipi/lcdc_mipi.c
    controller/spi/spi_only.c
)

ameba_list_append_if(CONFIG_AMEBAGREEN2 private_sources
    pal/${c_SOC_TYPE}/panel_pin_setting.c
)

ameba_list_append_if(CONFIG_AMEBASMART private_sources
    pal/${c_SOC_TYPE}/panel_pin_setting.c
)

ameba_list_append(private_includes
    controller/include
    ./
)

ameba_list_append(private_definitions
    __RTOS__
    NDEBUG
)

ameba_list_append(private_compile_options
    -UDEBUG
    -Wno-multichar
)
# Component private part, user config end
#------------------------------#

#WARNING: Select right API based on your component's release/not-release/standalone

###NOTE: For open-source component, always build from source
ameba_add_internal_library(lcd
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
