use std::{
    env,
};

fn main() {
    if env::var("DOCS_RS").is_ok() {
        println!("cargo:warning=Skipping spirv-to-dxil native build for docs.rs.");
        return;
    }

    let mut build = cc::Build::new();

    build
        .cpp(true)
        .std("c11")
        .define("HAVE_STRUCT_TIMESPEC", None)
        // We still should support Windows 7...
        .define("WINDOWS_NO_FUTEX", None)
        .define("_USE_MATH_DEFINES", None)
        .define("PACKAGE_VERSION", "\"100\"")
        .define("UTIL_ARCH_LITTLE_ENDIAN", None)
        .includes(&[
            "native/mesa/include",
            "native/mesa_mako",
            "native/mesa/src/util",
            "native/mesa/src/util/format",
            "native/mesa/src/util/sha1",

            "native/mesa/src",
            "native/mesa/src/compiler",
            "native/mesa/src/compiler/glsl",
            "native/mesa/src/compiler/nir",
            "native/mesa/src/compiler/spirv",
            "native/mesa/src/microsoft/compiler",

        ])
        .files(&[
            "native/mesa/src/c11/impl/time.c",

            "native/mesa/src/util/ralloc.c",
            "native/mesa/src/util/blob.c",
            "native/mesa/src/util/set.c",
            "native/mesa/src/util/hash_table.c",
            "native/mesa/src/util/u_worklist.c",
            "native/mesa/src/util/u_vector.c",
            "native/mesa/src/util/u_qsort.cpp",
            "native/mesa/src/util/u_debug.c",
            "native/mesa/src/util/u_dynarray.c",
            "native/mesa/src/util/u_printf.c",
            "native/mesa/src/util/u_call_once.c",
            "native/mesa/src/util/sha1/sha1.c",
            "native/mesa/src/util/mesa-sha1.c",

            "native/mesa/src/util/os_misc.c",
            "native/mesa/src/util/memstream.c",

            "native/mesa/src/util/futex.c",
            "native/mesa/src/util/simple_mtx.c",

            "native/mesa/src/util/log.c",
            "native/mesa/src/util/rgtc.c",
            "native/mesa/src/util/dag.c",
            "native/mesa/src/util/rb_tree.c",
            "native/mesa/src/util/string_buffer.c",

            "native/mesa/src/util/half_float.c",
            "native/mesa/src/util/softfloat.c",
            "native/mesa/src/util/double.c",
            "native/mesa/src/util/fast_idiv_by_const.c",

            "native/mesa/src/compiler/glsl_types.c",
            "native/mesa/src/compiler/shader_enums.c",

            "native/mesa/src/compiler/spirv/spirv_to_nir.c",
            "native/mesa/src/compiler/spirv/vtn_alu.c",
            "native/mesa/src/compiler/spirv/vtn_amd.c",
            "native/mesa/src/compiler/spirv/vtn_cfg.c",
            "native/mesa/src/compiler/spirv/vtn_cmat.c",
            "native/mesa/src/compiler/spirv/vtn_glsl450.c",
            "native/mesa/src/compiler/spirv/vtn_opencl.c",
            "native/mesa/src/compiler/spirv/vtn_structured_cfg.c",
            "native/mesa/src/compiler/spirv/vtn_subgroup.c",
            "native/mesa/src/compiler/spirv/vtn_variables.c",

            "native/mesa/src/microsoft/compiler/dxil_buffer.c",
            "native/mesa/src/microsoft/compiler/dxil_container.c",
            "native/mesa/src/microsoft/compiler/dxil_dump.c",
            "native/mesa/src/microsoft/compiler/dxil_enums.c",
            "native/mesa/src/microsoft/compiler/dxil_function.c",
            "native/mesa/src/microsoft/compiler/dxil_module.c",
            "native/mesa/src/microsoft/compiler/dxil_nir_lower_int_cubemaps.c",
            "native/mesa/src/microsoft/compiler/dxil_nir_lower_int_samplers.c",
            "native/mesa/src/microsoft/compiler/dxil_nir_lower_vs_vertex_conversion.c",
            "native/mesa/src/microsoft/compiler/dxil_nir_tess.c",
            "native/mesa/src/microsoft/compiler/dxil_nir.c",
            "native/mesa/src/microsoft/compiler/dxil_signature.c",
            "native/mesa/src/microsoft/compiler/nir_to_dxil.c",

            "native/mesa/src/microsoft/spirv_to_dxil/dxil_spirv_nir_lower_bindless.c",
            "native/mesa/src/microsoft/spirv_to_dxil/dxil_spirv_nir.c",
            "native/mesa/src/microsoft/spirv_to_dxil/spirv_to_dxil.c",
        ]);


    let compile_paths = &[
        "native/mesa_mako",
        "native/mesa/src/compiler/nir",
        "native/mesa/src/util/format"
    ];


    for &path in compile_paths {
        for file in std::fs::read_dir(path).unwrap() {
            let file = file.unwrap();
            let path = file.path();
            if path.extension().is_some_and(|s| s == "c") {
                build.file(path);
            }
        }
    }

    build
        .compile("spirv_to_dxil");

    println!("cargo:rustc-link-lib=static=spirv_to_dxil");

    if cfg!(target_os = "windows") {
        println!("cargo:rustc-link-lib=Version");
    }
    //
    // if cfg!(target_os = "linux") {
    //     let debian_arch = match env::var("CARGO_CFG_TARGET_ARCH").unwrap() {
    //         arch if arch == "x86" => "i386".to_owned(),
    //         arch => arch,
    //     };
    //
    //     let debian_triple_path = format!("/usr/lib/{}-linux-gnu/", debian_arch);
    //     let search_dir = if Path::new(&debian_triple_path).exists() {
    //         // Debian, Ubuntu and their derivatives.
    //         debian_triple_path
    //     } else if env::var("CARGO_CFG_TARGET_ARCH").unwrap() == "x86_64"
    //         && Path::new("/usr/lib64/").exists()
    //     {
    //         // Other distributions running on x86_64 usually use this path.
    //         "/usr/lib64/".to_string()
    //     } else {
    //         // Other distributions, not x86_64.
    //         "/usr/lib/".to_string()
    //     };
    //
    //     println!("cargo:rustc-link-search=native={}", search_dir);
    //     println!("cargo:rustc-link-lib=dylib=stdc++");
    // }
}
