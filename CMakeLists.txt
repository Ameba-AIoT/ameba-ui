ameba_add_subdirectories_if(CONFIG_LVGL_ENABLE LVGL)
ameba_add_subdirectory_if_exist(graphics)
ameba_add_subdirectory_if_exist(third_party)
ameba_add_subdirectory(drivers)

ameba_submodule_register(ui
p_EXAMPLE_DIRS
    examples/lv_demo_benchmark
    examples/lv_demo_stress
    examples/lv_demo_widgets
)
