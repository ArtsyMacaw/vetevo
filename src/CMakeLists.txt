# This file was automatically generated for projects
# without default 'CMakeLists.txt' file.

FILE(GLOB_RECURSE app_sources ${CMAKE_SOURCE_DIR}/src/*.*)

idf_component_register(SRCS ${app_sources}
                    EMBED_TXTFILES openweather.pem
                    INCLUDE_DIRS ""
                    REQUIRES nvs_flash ulp driver soc)

# ULP
set(ulp_app_name "ulp_main")
set(ulp_riscv_sources "../ulp/clock.c")
set(ulp_exp_dep_srcs "main.c")
ulp_embed_binary(${ulp_app_name} ${ulp_riscv_sources} ${ulp_exp_dep_srcs})
