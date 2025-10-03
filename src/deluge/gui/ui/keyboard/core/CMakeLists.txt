# Core Features Library CMakeLists.txt

add_library(deluge_keyboard_core STATIC
    core_features.cpp
)

target_include_directories(deluge_keyboard_core PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}
    ${CMAKE_CURRENT_LIST_DIR}/..
    ${CMAKE_CURRENT_LIST_DIR}/../..
    ${CMAKE_CURRENT_LIST_DIR}/../../../..
)

target_link_libraries(deluge_keyboard_core PUBLIC
    deluge_dsp
)
