cmake_minimum_required(VERSION 3.8)
project(native-spirv-to-dxil)
include(ExternalProject)

# need python
find_package(Python COMPONENTS Interpreter REQUIRED)
if(DEFINED ENV{VIRTUAL_ENV} OR DEFINED ENV{CONDA_PREFIX})
    set(_pip_args)
else()
    set(_pip_args "--user")
endif()

find_program(PIP_EXE pip)
if(PIP_EXE STREQUAL "PIP_EXE-NOTFOUND")
    message(FATAL_ERROR "pip required")
endif(PIP_EXE STREQUAL "PIP_EXE-NOTFOUND")

cmake_path(GET Python_EXECUTABLE PARENT_PATH PYTHON_DIR)
cmake_path(APPEND PYTHON_DIR "Scripts" OUTPUT_VARIABLE PYTHON_SCRIPTS_DIR)

execute_process(COMMAND ${Python_EXECUTABLE} -m pip install meson ninja mako)

find_program(MESON_EXE meson)
if(MESON_EXE STREQUAL "MESON_EXE-NOTFOUND")
    message(FATAL_ERROR "meson failed to install")
endif(MESON_EXE STREQUAL "MESON_EXE-NOTFOUND")

find_program(NINJA_EXE ninja)
if(NINJA_EXE STREQUAL "NINJA_EXE-NOTFOUND")
    message(FATAL_ERROR "ninja failed to install")
endif(NINJA_EXE STREQUAL "NINJA_EXE-NOTFOUND")

string(TOLOWER ${CMAKE_BUILD_TYPE} MESON_BUILD_TYPE)
set(MESA_EXTRA_FLAGS
        -Dbuildtype=${MESON_BUILD_TYPE}
        # enable spirv-to-dxil (obviously)
        -Dspirv-to-dxil=true
        # force a vulkan driver because we want libvulkan_util
        -Dvulkan-drivers=microsoft-experimental
        # set min-windows-version to 7 to remove need to link against synchronization.lib (we don't need futexes for the compiler)
        -Dmin-windows-version=7
        # disable everything we don't need.
        -Dgallium-drivers=
        -Dopengl=false
        -Dglx=disabled
        -Dmicrosoft-clc=disabled
        -Dgallium-d3d12-video=disabled
        -Dgbm=disabled
        -Degl=disabled
        -Dgles1=disabled
        -Dgles2=disabled
        -Dllvm=disabled
        -Dshared-llvm=disabled
        -Dplatforms=
        -Dzlib=disabled
        -Dshader-cache=disabled
        )

ExternalProject_Add(mesa
        URL file://${CMAKE_CURRENT_LIST_DIR}/mesa
        PREFIX ${CMAKE_BINARY_DIR}/mesa
        CONFIGURE_COMMAND cd ${CMAKE_BINARY_DIR}/mesa/src/mesa && meson setup ${CMAKE_BINARY_DIR}/mesa/src/mesa-build ${MESA_EXTRA_FLAGS}
        BUILD_COMMAND cd ${CMAKE_BINARY_DIR}/mesa/src/mesa-build && ninja src/vulkan/util/libvulkan_util.a && ninja src/microsoft/spirv_to_dxil/libspirv_to_dxil.a
        INSTALL_COMMAND cmake -DOUT_DIR=${CMAKE_BINARY_DIR}/mesa -DMESA_BUILD=${CMAKE_BINARY_DIR}/mesa/src/mesa-build -P ${CMAKE_CURRENT_LIST_DIR}/install_spirv_to_dxil.cmake
        BUILD_BYPRODUCTS ${CMAKE_BINARY_DIR}/mesa/lib/spirv_to_dxil.lib ${CMAKE_BINARY_DIR}/mesa/lib/libspirv_to_dxil.a ${CMAKE_BINARY_DIR}/mesa/lib/vulkan_util.lib ${CMAKE_BINARY_DIR}/mesa/lib/libvulkan_util.a
        )

