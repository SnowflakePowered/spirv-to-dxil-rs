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
        .std("c11")
        .define("HAVE_STRUCT_TIMESPEC", None)

        .define("PACKAGE_VERSION", "\"100\"")
        .define("__STDC_CONSTANT_MACROS", None)
        .define("__STDC_FORMAT_MACROS", None)
        .define("__STDC_LIMIT_MACROS", None)
        .define("M_E", "2.71828182845904523536")
        .define("M_LOG2E", "1.44269504088896340736")
        .define("M_LOG10E", "0.434294481903251827651")
        .define("M_LN2", "0.693147180559945309417")
        .define("M_LN10", "2.30258509299404568402")
        .define("M_PI", "3.14159265358979323846")
        .define("M_PI_2", "1.57079632679489661923")
        .define("M_PI_4", "0.785398163397448309616")
        .define("M_1_PI", "0.318309886183790671538")
        .define("M_2_PI", "0.636619772367581343076")
        .define("M_2_SQRTPI", "1.12837916709551257390")
        .define("M_SQRT2", "1.41421356237309504880")
        .define("M_SQRT1_2", "0.707106781186547524401")
        .flag_if_supported("-fpermissive")
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

    if cfg!(target_os = "windows") {
        build
            // We still should support Windows 7...
            .define("WINDOWS_NO_FUTEX", None)
            .file("native/mesa/src/c11/impl/threads_win32.c");

    } else {
        build
            .file("native/mesa/src/c11/impl/threads_posix.c")
            .define("HAVE_PTHREAD", None)
            .define("_POSIX_SOURCE", None)
            .define("_GNU_SOURCE", None)
        ;
    }

    if cfg!(target_endian = "big") {
        build.define(
            "UTIL_ARCH_BIG_ENDIAN", "1"
        ).define("UTIL_ARCH_LITTLE_ENDIAN", "0");
    } else {
        build.define(
            "UTIL_ARCH_BIG_ENDIAN", "0"
        ).define("UTIL_ARCH_LITTLE_ENDIAN", "1");
    };

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
}
