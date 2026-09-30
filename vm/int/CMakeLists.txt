# Set the minimum version of CMake that can be used
# To find the cmake version run
# $ cmake --version
cmake_minimum_required(VERSION 3.5)

# Set the project name
project (kronos_vm)

find_package(SDL2 REQUIRED CONFIG REQUIRED COMPONENTS SDL2)

# Create a sources variable with a link to all cpp files to compile
set(SOURCES
    src/Disks.cpp
    src/Kronos3vm.cpp
    src/Memory.cpp
    src/preCompiled.cpp
    src/SIO_Mouse.cpp
    src/VM.cpp
    src/vmConsole.cpp
    src/SIO.cpp
    src/SIO_TCP.cpp
    src/cO_lin.cpp
    src/cO_lin_tcp.cpp
)

# Add an executable with the above sources
add_executable(kronos_vm ${SOURCES})

# Set the directories that should be included in the build command for this target
# when running g++ these will be included as -I/directory/path/
target_include_directories(kronos_vm
    PRIVATE
        ${PROJECT_SOURCE_DIR}/src
)

install(TARGETS kronos_vm
    DESTINATION ${PROJECT_SOURCE_DIR}/bin
)

target_link_libraries(kronos_vm PRIVATE SDL2::SDL2)
