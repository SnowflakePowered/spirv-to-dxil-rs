# spirv-to-dxil-rs

Safe Rust bindings to [spirv-to-dxil](https://gitlab.freedesktop.org/mesa/mesa/-/blob/main/src/microsoft/spirv_to_dxil/spirv_to_dxil.h).

[![Latest Version](https://img.shields.io/crates/v/spirv-to-dxil.svg)](https://crates.io/crates/spirv-to-dxil) [![Docs](https://docs.rs/spirv-to-dxil/badge.svg)](https://docs.rs/spirv-to-dxil) ![License](https://img.shields.io/crates/l/spirv-to-dxil)

## Building

spirv-to-dxil-rs builds a copy of spirv-to-dxil statically from Mesa. 

* A compatible C and C++ compiler
  * [MSVC 2019 16.11 or later](https://docs.mesa3d.org/install.html) is required to build on Windows.

  
A script to clone a minimal subset of Mesa required to build spirv-to-dxil has been provided. 

```bash
$ ./clone-mesa.sh
$ cargo build
```

## Updating Mesa

Unless you are maintaining spirv-to-dxil-rs, you do not need to update Mesa frequently.

Updating Mesa requires Python 3.6 and mako to pre-generate templated files. CMake is also required to
regenerate the Rust bindings from `spirv-to-dxil.h`, this is to ensure that bindings can be generated
regardless of whether changes need to be made to the `spirv-to-dxil-sys` build script.

1. Update the submodule to HEAD

   `git submodule update --init --remote --depth 1 --single-branch --progress spirv-to-dxil-sys/native/mesa`
2. Re-apply sparse-checkout
    ```bash
   git submodule absorbgitdirs
   git -C spirv-to-dxil-sys/native/mesa config core.sparseCheckout true
   git -C spirv-to-dxil-sys/native/mesa config core.symlinks false
   cp spirv-to-dxil-sys/native/mesa-sparse-checkout .git/modules/spirv-to-dxil-sys/native/mesa/info/sparse-checkout
   git submodule foreach git sparse-checkout reapply
    ```
3. Regenerate bindings
   ```bash
   cargo run --bin bindings_generator
   ```
4. Regenerate mako-generated files for `cc`
   ```bash
   ./mesa_mako 
   ```
