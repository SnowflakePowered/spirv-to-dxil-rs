git submodule init spirv-to-dxil-sys/native/mesa
git submodule update --init --depth 1 --single-branch --progress spirv-to-dxil-sys/native/mesa
git submodule absorbgitdirs

git -C spirv-to-dxil-sys/native/mesa config core.sparseCheckout true
git -C spirv-to-dxil-sys/native/mesa config core.symlinks false
cp spirv-to-dxil-sys/native/mesa-sparse-checkout .git/modules/spirv-to-dxil-sys/native/mesa/info/sparse-checkout
git submodule foreach git sparse-checkout reapply
