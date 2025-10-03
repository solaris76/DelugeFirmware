# Keyboard Modules CMakeLists.txt
# This file builds the modular keyboard system

# Add module source files
set(KEYBOARD_MODULES_SOURCES
    src/deluge/gui/ui/keyboard/modules/module_base.cpp
    src/deluge/gui/ui/keyboard/modules/module_manager.cpp
    src/deluge/gui/ui/keyboard/modules/timing_module.cpp
    src/deluge/gui/ui/keyboard/modules/test_module.cpp
    src/deluge/gui/ui/keyboard/modules/core_features_module.cpp
    src/deluge/gui/ui/keyboard/modules/obvious_test_module.cpp
    src/deluge/gui/ui/keyboard/modules/generative_sequencer_module.cpp
    src/deluge/gui/ui/keyboard/modules/module_control.cpp
)

# Add module header files
set(KEYBOARD_MODULES_HEADERS
    src/deluge/gui/ui/keyboard/modules/module_base.h
    src/deluge/gui/ui/keyboard/modules/module_manager.h
    src/deluge/gui/ui/keyboard/modules/timing_module.h
    src/deluge/gui/ui/keyboard/modules/test_module.h
    src/deluge/gui/ui/keyboard/modules/core_features_module.h
    src/deluge/gui/ui/keyboard/modules/obvious_test_module.h
    src/deluge/gui/ui/keyboard/modules/generative_sequencer_module.h
    src/deluge/gui/ui/keyboard/modules/module_control.h
)

# Create module library
add_library(keyboard_modules STATIC ${KEYBOARD_MODULES_SOURCES})

# Link with required libraries
target_link_libraries(keyboard_modules
    deluge
    ${CMAKE_THREAD_LIBS_INIT}
)

# Include directories
target_include_directories(keyboard_modules PUBLIC
    src/deluge/gui/ui/keyboard/modules
)

# Compiler definitions
target_compile_definitions(keyboard_modules PRIVATE
    KEYBOARD_MODULES_ENABLED=1
)

# Set C++ standard
set_target_properties(keyboard_modules PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)
