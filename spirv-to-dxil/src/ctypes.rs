/// Configuration options for the runtime data constant buffer if required.
pub use spirv_to_dxil_sys::dxil_spirv_runtime_conf_runtime_cbv as RuntimeDataBufferConfig;

/// Configuration options for the push constant buffer if required
pub use spirv_to_dxil_sys::dxil_spirv_runtime_conf_push_cbv as PushConstantBufferConfig;

/// Configuration options for SPIR-V YZ flip mode.
pub use spirv_to_dxil_sys::dxil_spirv_runtime_conf_flip_conf as FlipConfig;

/// Runtime configuration options for the SPIR-V compilation.
pub use spirv_to_dxil_sys::dxil_spirv_runtime_conf as RuntimeConfig;

/// The DXIL shader model to target.
pub use spirv_to_dxil_sys::dxil_shader_model as ShaderModel;

/// The validator version to target. Requires `dxil.dll` in path.
pub use spirv_to_dxil_sys::dxil_validator_version as ValidatorVersion;

/// The shader stage of the input.
pub use spirv_to_dxil_sys::dxil_spirv_shader_stage as ShaderStage;

/// The flip mode to use when compiling the shader.
pub use spirv_to_dxil_sys::dxil_spirv_yz_flip_mode as FlipMode;