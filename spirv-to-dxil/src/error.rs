use thiserror::Error;
use spirv_to_dxil_sys::ShaderModel;

#[derive(Debug, Error)]
/// Error type for spirv-to-dxil.
pub enum SpirvToDxilError {
    /// An error occurred when compiling SPIR-V to DXIL.
    #[error("An error occurred when compiled to DXIL: {0}.")]
    CompilerError(String),
    /// The requested register space for a push or runtime constant block is beyond
    /// the limit of 31.
    #[error("Register space {0} is beyond the limit of 31.")]
    RegisterSpaceOverflow(u32),
    /// The requested shader model is invalid for this output type
    #[error("Shader model {0} is not supported.")]
    InvalidShaderModel(ShaderModel),
}
