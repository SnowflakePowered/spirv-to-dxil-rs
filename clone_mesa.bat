@echo off
git submodule init spirv-to-dxil-sys/native/mesa
git submodule update --init --filter=blob:none --depth 1 --single-branch --progress spirv-to-dxil-sys/native/mesa
git submodule absorbgitdirs spirv-to-dxil-sys/native/mesa

git -C spirv-to-dxil-sys/native/mesa config core.sparseCheckout true
git -C spirv-to-dxil-sys/native/mesa config core.symlinks false
copy spirv-to-dxil-sys/native/mesa-sparse-checkout .git/modules/spirv-to-dxil-sys/native/mesa/info/sparse-checkout
git submodule foreach git sparse-checkout reapply

