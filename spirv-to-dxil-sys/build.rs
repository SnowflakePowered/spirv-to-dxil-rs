use std::{fs::File, env, path::PathBuf};
use cmake::Config;

fn main() {
    let out_dir = PathBuf::from(env::var("OUT_DIR").unwrap());
    if env::var("DOCS_RS").is_ok() {
            println!("cargo:warning=Skipping spirv-to-dxil native build for docs.rs.");
            File::create(out_dir.join("bindings.rs")).unwrap();
            return;
    }

    let cmake_dst = Config::new("native")
        .build_target("mesa")
        .build();

    let object_dst = cmake_dst.join("build/mesa/lib");

    // let vulkan_util_dst = cmake_dst.join("build/mesa/src/mesa/src/vulkan/util");
    let header_dst = cmake_dst.join("build/mesa/src/mesa/src/microsoft/spirv_to_dxil");
    let header_compiler_dst = cmake_dst.join("build/mesa/src/mesa/src/microsoft/compiler");

    if cfg!(target_os = "windows") {
        println!("cargo:rustc-link-lib=Version");
        println!("cargo:rustc-link-lib=synchronization");
    }

    println!("cargo:rustc-link-search=native={}", object_dst.display());
    println!("cargo:rustc-link-lib=static=spirv_to_dxil");
    println!("cargo:rustc-link-lib=static=vulkan_util");
    eprintln!("{:?}", cmake_dst);

    let bindings = bindgen::Builder::default()
        .header("native/wrapper.h")
        .clang_arg(format!("-F{}", header_dst.display()))
        .clang_arg(format!("-F{}", header_compiler_dst.display()))
        .parse_callbacks(Box::new(bindgen::CargoCallbacks))
        .generate()
        .expect("Unable to generate bindings");
    bindings
        .write_to_file(out_dir.join("bindings.rs"))
        .expect("Couldn't write bindings!");

}