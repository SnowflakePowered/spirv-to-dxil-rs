git submodule init spirv-to-dxil-sys/native/mesa
git submodule update --init --filter=blob:none --depth 1 --single-branch --progress spirv-to-dxil-sys/native/mesa
git submodule absorbgitdirs

git -C spirv-to-dxil-sys/native/mesa config core.sparseCheckout true
git -C spirv-to-dxil-sys/native/mesa config core.symlinks false
cp spirv-to-dxil-sys/native/mesa-sparse-checkout .git/modules/spirv-to-dxil-sys/native/mesa/info/sparse-checkout
git submodule foreach git sparse-checkout reapply

mkdir -p spirv-to-dxil-sys/native/mesa_mako

python spirv-to-dxil-sys/native/mesa/src/compiler/builtin_types_h.py spirv-to-dxil-sys/native/mesa_mako/builtin_types.h
python spirv-to-dxil-sys/native/mesa/src/compiler/builtin_types_c.py spirv-to-dxil-sys/native/mesa_mako/builtin_types.c
python spirv-to-dxil-sys/native/mesa/src/compiler/builtin_types_cpp_h.py spirv-to-dxil-sys/native/mesa_mako/builtin_types_cpp.h

python spirv-to-dxil-sys/native/mesa/src/compiler/spirv/spirv_info_c.py spirv-to-dxil-sys/native/mesa/src/compiler/spirv/spirv.core.grammar.json spirv-to-dxil-sys/native/mesa_mako/spirv_info.c
python spirv-to-dxil-sys/native/mesa/src/compiler/spirv/vtn_gather_types_c.py spirv-to-dxil-sys/native/mesa/src/compiler/spirv/spirv.core.grammar.json spirv-to-dxil-sys/native/mesa_mako/vtn_gather_types.c
python spirv-to-dxil-sys/native/mesa/src/compiler/spirv/vtn_generator_ids_h.py spirv-to-dxil-sys/native/mesa/src/compiler/spirv/spir-v.xml spirv-to-dxil-sys/native/mesa_mako/vtn_generator_ids.h


python spirv-to-dxil-sys/native/mesa/src/compiler/nir/nir_builder_opcodes_h.py > spirv-to-dxil-sys/native/mesa_mako/nir_builder_opcodes.h
python spirv-to-dxil-sys/native/mesa/src/compiler/nir/nir_constant_expressions.py > spirv-to-dxil-sys/native/mesa_mako/nir_constant_expressions.c

python spirv-to-dxil-sys/native/mesa/src/compiler/nir/nir_opcodes_h.py > spirv-to-dxil-sys/native/mesa_mako/nir_opcodes.h
python spirv-to-dxil-sys/native/mesa/src/compiler/nir/nir_opcodes_c.py > spirv-to-dxil-sys/native/mesa_mako/nir_opcodes.c
python spirv-to-dxil-sys/native/mesa/src/compiler/nir/nir_opt_algebraic.py > spirv-to-dxil-sys/native/mesa_mako/nir_opt_algebraic.c
python spirv-to-dxil-sys/native/mesa/src/compiler/nir/nir_intrinsics_h.py --outdir spirv-to-dxil-sys/native/mesa_mako
python spirv-to-dxil-sys/native/mesa/src/compiler/nir/nir_intrinsics_c.py --outdir spirv-to-dxil-sys/native/mesa_mako
python spirv-to-dxil-sys/native/mesa/src/compiler/nir/nir_intrinsics_indices_h.py --outdir spirv-to-dxil-sys/native/mesa_mako

python spirv-to-dxil-sys/native/mesa/src/util/format_srgb.py > spirv-to-dxil-sys/native/mesa_mako/format_srgb.c
python spirv-to-dxil-sys/native/mesa/src/util/format/u_format_table.py spirv-to-dxil-sys/native/mesa/src/util/format/u_format.csv > spirv-to-dxil-sys/native/mesa_mako/u_format_table.c
python spirv-to-dxil-sys/native/mesa/src/util/format/u_format_table.py spirv-to-dxil-sys/native/mesa/src/util/format/u_format.csv --header > spirv-to-dxil-sys/native/mesa_mako/u_format_pack.h

python spirv-to-dxil-sys/native/mesa/src/microsoft/compiler/dxil_nir_algebraic.py -p spirv-to-dxil-sys/native/mesa/src/compiler/nir/ > spirv-to-dxil-sys/native/mesa_mako/dxil_nir_algebraic.c
