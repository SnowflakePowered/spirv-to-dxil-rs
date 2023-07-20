@echo off
git clone --filter=blob:none --no-checkout https://gitlab.freedesktop.org/mesa/mesa
git submodule add https://gitlab.freedesktop.org/mesa/mesa spirv-to-dxil-sys/native/mesa
git submodule absorbgitdirs

git -C spirv-to-dxil-sys/native/mesa config core.sparseCheckout true
git -C spirv-to-dxil-sys/native/mesa config core.symlinks false

copy ./spirv-to-dxil-sys/native/mesa-sparse-checkout ./.git/modules/spirv-to-dxil-sys/native/mesa/info
git submodule update --init --force --checkout spirv-to-dxil-sys/native/mesa
