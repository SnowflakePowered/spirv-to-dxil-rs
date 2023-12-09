# spirv-to-dxil-rs

Safe Rust bindings to [spirv-to-dxil](https://gitlab.freedesktop.org/mesa/mesa/-/blob/main/src/microsoft/spirv_to_dxil/spirv_to_dxil.h).

[![Latest Version](https://img.shields.io/crates/v/spirv-to-dxil.svg)](https://crates.io/crates/spirv-to-dxil) [![Docs](https://docs.rs/spirv-to-dxil/badge.svg)](https://docs.rs/spirv-to-dxil) ![License](https://img.shields.io/crates/l/spirv-to-dxil)

## Building

spirv-to-dxil-rs builds a copy of spirv-to-dxil statically from Mesa. 

* A compatible C and C++ compiler
  * [MSVC 2019 16.11 or later](https://docs.mesa3d.org/install.html) is required to build on Windows.



A script to clone a minimal subset of Mesa required to build spirv-to-dxil has been provided. 
This requires Python and mako to pre-generate templated files.

```bash
$ ./clone-mesa.sh
$ cargo build
```

