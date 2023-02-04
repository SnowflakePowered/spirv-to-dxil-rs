# spirv-to-dxil-rs

Safe Rust bindings to [spirv-to-dxil](https://gitlab.freedesktop.org/mesa/mesa/-/blob/main/src/microsoft/spirv_to_dxil/spirv_to_dxil.h).

## Building

spirv-to-dxil-rs builds a copy of spirv-to-dxil statically from Mesa. Many of the build requirements are [the same as needed to build Mesa](https://docs.mesa3d.org/install.html).

* [Meson](https://mesonbuild.com/)
* A compatible C and C++ compiler
  * [MSVC 2019 16.11 or later](https://docs.mesa3d.org/install.html) is required to build on Windows.

* [Python 3.6](https://www.python.org/) or later.

Lex and Yacc are not required. Additionally, [CMake 3.6](https://cmake.org/) or later is required to run the build script.

```bash
$ git submodule update --init --progress --depth=1
$ cargo build
```

