/// The shader stage of the input.
pub use spirv_to_dxil_sys::ShaderStage;

/// Configuration options for SPIR-V YZ flip mode.
pub use spirv_to_dxil_sys::dxil_spirv_runtime_conf_flip_conf as FlipConfig;

/// Runtime configuration options for the SPIR-V compilation.
pub use spirv_to_dxil_sys::dxil_spirv_runtime_conf as DxilRuntimeConfig;

/// Runtime configuration options for the SPIR-V compilation.
pub use spirv_to_dxil_sys::dxbc_spirv_runtime_conf as DxbcRuntimeConfig;

/// The DXIL shader model to target.
pub use spirv_to_dxil_sys::ShaderModel;

/// The validator version to target. Requires `dxil.dll` in path.
pub use spirv_to_dxil_sys::ValidatorVersion;

/// The flip mode to use when compiling the shader.
pub use spirv_to_dxil_sys::dxil_spirv_yz_flip_mode as FlipMode;
