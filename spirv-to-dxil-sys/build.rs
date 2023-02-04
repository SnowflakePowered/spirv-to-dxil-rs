use cmake::Config;
use std::{env, fs::File, path::{Path, PathBuf}};

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

    let header_dst = cmake_dst.join("build/mesa/src/mesa/src/microsoft/spirv_to_dxil");
    let header_compiler_dst = cmake_dst.join("build/mesa/src/mesa/src/microsoft/compiler");

    println!("cargo:rustc-link-search=native={}", object_dst.display());
    println!("cargo:rustc-link-lib=static=spirv_to_dxil");

    if cfg!(target_os = "windows") {
        println!("cargo:rustc-link-lib=Version");
        // only Windows needs to link vulkan_util.lib
        println!("cargo:rustc-link-lib=static=vulkan_util");
    }

    if cfg!(target_os = "linux") {
        let debian_arch = match env::var("CARGO_CFG_TARGET_ARCH").unwrap() {
            arch if arch == "x86" => "i386".to_owned(),
            arch => arch,
        };

        let debian_triple_path = format!("/usr/lib/{}-linux-gnu/", debian_arch);
        let search_dir = if Path::new(&debian_triple_path).exists() {
            // Debian, Ubuntu and their derivatives.
            debian_triple_path
        } else if env::var("CARGO_CFG_TARGET_ARCH").unwrap() == "x86_64"
            && Path::new("/usr/lib64/").exists()
        {
            // Other distributions running on x86_64 usually use this path.
            "/usr/lib64/".to_string()
        } else {
            // Other distributions, not x86_64.
            "/usr/lib/".to_string()
        };

        println!("cargo:rustc-link-search=native={}", search_dir);
        println!("cargo:rustc-link-lib=dylib=stdc++");
    }



    let bindings = bindgen::Builder::default()
        .header("native/wrapper.h")
        .clang_arg(format!("-F{}", header_dst.display()))
        .clang_arg(format!("-F{}", header_compiler_dst.display()))
        .clang_arg(format!("-I{}", header_dst.display()))
        .clang_arg(format!("-I{}", header_compiler_dst.display()))
        .parse_callbacks(Box::new(bindgen::CargoCallbacks))
        .generate()
        .expect("Unable to generate bindings");
    bindings
        .write_to_file(out_dir.join("bindings.rs"))
        .expect("Couldn't write bindings!");
}
