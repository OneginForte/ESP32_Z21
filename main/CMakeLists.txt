# This file was automatically generated for projects
# without default 'CMakeLists.txt' file.

FILE(GLOB_RECURSE app_sources ${CMAKE_SOURCE_DIR}/main/*.*)

idf_component_register(SRCS ${app_sources} 
                      INCLUDE_DIRS ${CMAKE_SOURCE_DIR}/include/
                      REQUIRES log nvs_flash mdns wpa_supplicant lwip esp_http_server esp32-wifi-manager
                      
                      )

