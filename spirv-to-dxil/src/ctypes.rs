/// The shader stage of the input.
pub use spirv_to_dxil_sys::ShaderStage;

/// The DXIL shader model to target.
pub use spirv_to_dxil_sys::ShaderModel;

/// The validator version to target. Requires `dxil.dll` in path.
pub use spirv_to_dxil_sys::ValidatorVersion;

/// The binding for a runtime buffer required for this shader.
pub use spirv_to_dxil_sys::BufferBinding;
