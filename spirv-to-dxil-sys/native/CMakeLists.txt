cmake_minimum_required(VERSION 3.6)
project(native-spirv-to-dxil)
include(ExternalProject)


# need python
find_package(Python COMPONENTS Interpreter REQUIRED)
if(DEFINED ENV{VIRTUAL_ENV} OR DEFINED ENV{CONDA_PREFIX})
    set(_pip_args)
else()
    set(_pip_args "--user")
endif()

execute_process(COMMAND ${Python_EXECUTABLE} -m pip install mako)

find_program(LEX_EXE
        flex
        )
if(LEX_EXE STREQUAL "LEX_EXE-NOTFOUND")
    message(FATAL_ERROR "flex (lex) required to build NIR and GLSL compiler")
endif(LEX_EXE STREQUAL "LEX_EXE-NOTFOUND")

find_program(YACC_EXE
        bison
        )
if(YACC_EXE STREQUAL "YACC_EXE-NOTFOUND")
    message(FATAL_ERROR "bison (yacc) required to build NIR and GLSL compiler")
endif(YACC_EXE STREQUAL "YACC_EXE-NOTFOUND")

set(MESA_EXTRA_FLAGS
        -Dbuildtype=release
        -Dgallium-drivers=
        -Dglx=disabled
        -Dgbm=disabled
        -Degl=disabled
        -Dgles1=disabled
        -Dgles2=disabled
        -Dshared-llvm=disabled
        -Dspirv-to-dxil=true
        -Dplatforms=auto
        )


ExternalProject_Add(mesa
        URL file://${CMAKE_CURRENT_LIST_DIR}/mesa
        PREFIX ${CMAKE_BINARY_DIR}/mesa
        CONFIGURE_COMMAND cd ${CMAKE_BINARY_DIR}/mesa/src/mesa && meson setup ${CMAKE_BINARY_DIR}/mesa/src/mesa-build ${MESA_EXTRA_FLAGS}
        BUILD_COMMAND cd ${CMAKE_BINARY_DIR}/mesa/src/mesa-build && ninja src/microsoft/spirv_to_dxil/libspirv_to_dxil.a
        INSTALL_COMMAND cmake -DOUT_DIR=${CMAKE_BINARY_DIR}/mesa -DMESA_BUILD=${CMAKE_BINARY_DIR}/mesa/src/mesa-build -P ${CMAKE_CURRENT_LIST_DIR}/install_spirv_to_dxil.cmake
        BUILD_BYPRODUCTS ${CMAKE_BINARY_DIR}/mesa/lib/spirv_to_dxil.lib ${CMAKE_BINARY_DIR}/mesa/lib/libspirv_to_dxil.a
        )

