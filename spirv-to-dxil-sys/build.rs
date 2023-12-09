use cmake::Config;
use std::{
    env,
    path::Path
};

fn main() {
    if env::var("DOCS_RS").is_ok() {
        println!("cargo:warning=Skipping spirv-to-dxil native build for docs.rs.");
        return;
    }

    let cmake_dst = Config::new("native")
        .profile("Release")
        .build_target("mesa")
        .build();

    let object_dst = cmake_dst.join("build/mesa/lib");

    println!("cargo:rustc-link-search=native={}", object_dst.display());
    println!("cargo:rustc-link-lib=static=spirv_to_dxil");

    if cfg!(target_os = "windows") {
        println!("cargo:rustc-link-lib=Version");
        // only Windows needs to link vulkan_util.lib
        // println!("cargo:rustc-link-lib=static=vulkan_util");
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
}
