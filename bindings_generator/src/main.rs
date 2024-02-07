mod callbacks;

use cmake::Config;
use std::env;

fn main() {
    let cmake_dst = Config::new("spirv-to-dxil-sys/native")
        .profile("Release")
        .build_target("mesa")
        .build();

    let header_dst = cmake_dst.join("build/mesa/src/mesa/src/microsoft/spirv_to_dxil");
    let header_compiler_dst = cmake_dst.join("build/mesa/src/mesa/src/microsoft/compiler");

    let bindings = bindgen::Builder::default()
        .header("spirv-to-dxil-sys/native/wrapper.h")
        .clang_arg(format!("-F{}", header_dst.display()))
        .clang_arg(format!("-F{}", header_compiler_dst.display()))
        .clang_arg(format!("-I{}", header_dst.display()))
        .clang_arg(format!("-I{}", header_compiler_dst.display()))
        .parse_callbacks(Box::new(callbacks::SpirvToDxilCallbacks))
        .allowlist_var("DXIL_SPIRV_MAX_VIEWPORT")
        .allowlist_type("dxil_spirv_.*")
        .allowlist_type("dxil_shader_model")
        .allowlist_type("dxil_validator_version")
        .allowlist_function("spirv_to_dxil")
        .allowlist_function("spirv_to_dxil_free")
        .allowlist_function("spirv_to_dxil_get_version")
        .rustified_enum("dxil_spirv_shader_stage")
        .rustified_non_exhaustive_enum("dxil_shader_model")
        .rustified_non_exhaustive_enum("dxil_validator_version")
        .bitfield_enum("dxil_spirv_yz_flip_mode")
        .anon_fields_prefix("_dxil_spirv_anon")
        .generate()
        .expect("Unable to generate bindings");
    bindings
        .write_to_file(
            env::current_dir()
                .unwrap()
                .join("spirv-to-dxil-sys/src/bindings.rs"),
        )
        .expect("Couldn't write bindings!");
}
