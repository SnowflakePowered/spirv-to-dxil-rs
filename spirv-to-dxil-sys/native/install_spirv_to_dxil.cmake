file(INSTALL ${MESA_BUILD}/src/microsoft/spirv_to_dxil/libspirv_to_dxil.a DESTINATION ${OUT_DIR}/lib)

if (WIN32)
    file(RENAME ${OUT_DIR}/lib/libspirv_to_dxil.a ${OUT_DIR}/lib/spirv_to_dxil.lib)
endif()
