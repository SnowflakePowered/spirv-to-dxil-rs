use bitflags::bitflags;

pub enum ShaderStage {
    None,
    Vertex,
    TesselationControl,
    TesselationEvaluation,
    Geometry,
    Fragment,
    Compute,
    Kernel,
}

impl From<ShaderStage> for spirv_to_dxil_sys::dxil_spirv_shader_stage {
    fn from(value: ShaderStage) -> Self {
        match value {
            ShaderStage::None => spirv_to_dxil_sys::dxil_spirv_shader_stage_DXIL_SPIRV_SHADER_NONE,
            ShaderStage::Vertex => {
                spirv_to_dxil_sys::dxil_spirv_shader_stage_DXIL_SPIRV_SHADER_VERTEX
            }
            ShaderStage::TesselationControl => {
                spirv_to_dxil_sys::dxil_spirv_shader_stage_DXIL_SPIRV_SHADER_TESS_CTRL
            }
            ShaderStage::TesselationEvaluation => {
                spirv_to_dxil_sys::dxil_spirv_shader_stage_DXIL_SPIRV_SHADER_TESS_EVAL
            }
            ShaderStage::Geometry => {
                spirv_to_dxil_sys::dxil_spirv_shader_stage_DXIL_SPIRV_SHADER_GEOMETRY
            }
            ShaderStage::Fragment => {
                spirv_to_dxil_sys::dxil_spirv_shader_stage_DXIL_SPIRV_SHADER_FRAGMENT
            }
            ShaderStage::Compute => {
                spirv_to_dxil_sys::dxil_spirv_shader_stage_DXIL_SPIRV_SHADER_COMPUTE
            }
            ShaderStage::Kernel => {
                spirv_to_dxil_sys::dxil_spirv_shader_stage_DXIL_SPIRV_SHADER_KERNEL
            }
        }
    }
}

#[non_exhaustive]
pub enum ShaderModel {
    ShaderModel6_0,
    ShaderModel6_1,
    ShaderModel6_2,
    ShaderModel6_3,
    ShaderModel6_4,
    ShaderModel6_5,
    ShaderModel6_6,
    ShaderModel6_7,
}

impl From<ShaderModel> for spirv_to_dxil_sys::dxil_shader_model {
    fn from(value: ShaderModel) -> Self {
        match value {
            ShaderModel::ShaderModel6_0 => spirv_to_dxil_sys::dxil_shader_model_SHADER_MODEL_6_0,
            ShaderModel::ShaderModel6_1 => spirv_to_dxil_sys::dxil_shader_model_SHADER_MODEL_6_1,
            ShaderModel::ShaderModel6_2 => spirv_to_dxil_sys::dxil_shader_model_SHADER_MODEL_6_2,
            ShaderModel::ShaderModel6_3 => spirv_to_dxil_sys::dxil_shader_model_SHADER_MODEL_6_3,
            ShaderModel::ShaderModel6_4 => spirv_to_dxil_sys::dxil_shader_model_SHADER_MODEL_6_4,
            ShaderModel::ShaderModel6_5 => spirv_to_dxil_sys::dxil_shader_model_SHADER_MODEL_6_5,
            ShaderModel::ShaderModel6_6 => spirv_to_dxil_sys::dxil_shader_model_SHADER_MODEL_6_6,
            ShaderModel::ShaderModel6_7 => spirv_to_dxil_sys::dxil_shader_model_SHADER_MODEL_6_7,
        }
    }
}

#[non_exhaustive]
pub enum ValidatorVersion {
    None,
    Validator1_0,
    Validator1_1,
    Validator1_2,
    Validator1_3,
    Validator1_4,
    Validator1_5,
    Validator1_6,
    Validator1_7,
}

impl From<ValidatorVersion> for spirv_to_dxil_sys::dxil_validator_version {
    fn from(value: ValidatorVersion) -> Self {
        match value {
            ValidatorVersion::None => spirv_to_dxil_sys::dxil_validator_version_NO_DXIL_VALIDATION,
            ValidatorVersion::Validator1_0 => {
                spirv_to_dxil_sys::dxil_validator_version_DXIL_VALIDATOR_1_0
            }
            ValidatorVersion::Validator1_1 => {
                spirv_to_dxil_sys::dxil_validator_version_DXIL_VALIDATOR_1_1
            }
            ValidatorVersion::Validator1_2 => {
                spirv_to_dxil_sys::dxil_validator_version_DXIL_VALIDATOR_1_2
            }
            ValidatorVersion::Validator1_3 => {
                spirv_to_dxil_sys::dxil_validator_version_DXIL_VALIDATOR_1_3
            }
            ValidatorVersion::Validator1_4 => {
                spirv_to_dxil_sys::dxil_validator_version_DXIL_VALIDATOR_1_4
            }
            ValidatorVersion::Validator1_5 => {
                spirv_to_dxil_sys::dxil_validator_version_DXIL_VALIDATOR_1_5
            }
            ValidatorVersion::Validator1_6 => {
                spirv_to_dxil_sys::dxil_validator_version_DXIL_VALIDATOR_1_6
            }
            ValidatorVersion::Validator1_7 => {
                spirv_to_dxil_sys::dxil_validator_version_DXIL_VALIDATOR_1_7
            }
        }
    }
}

bitflags! {
    #[derive(Default)]
    pub struct FlipMode: spirv_to_dxil_sys::dxil_spirv_yz_flip_mode {
        /// No YZ flip
        const NONE = spirv_to_dxil_sys::dxil_spirv_yz_flip_mode_DXIL_SPIRV_YZ_FLIP_NONE;
        /// Y-flip is unconditional: pos.y = -pos.y
        const Y_FLIP_UNCONDITIONAL = spirv_to_dxil_sys::dxil_spirv_yz_flip_mode_DXIL_SPIRV_Y_FLIP_UNCONDITIONAL;
        /// Z-flip is unconditional: pos.z = -pos.z + 1.0f
        const Z_FLIP_UNCONDITIONAL = spirv_to_dxil_sys::dxil_spirv_yz_flip_mode_DXIL_SPIRV_Z_FLIP_UNCONDITIONAL;
        const YZ_FLIP_UNCONDITIONAL = spirv_to_dxil_sys::dxil_spirv_yz_flip_mode_DXIL_SPIRV_YZ_FLIP_UNCONDITIONAL;
        const Y_FLIP_CONDITIONAL = spirv_to_dxil_sys::dxil_spirv_yz_flip_mode_DXIL_SPIRV_Y_FLIP_CONDITIONAL;
        /// Z-flip is unconditional: pos.z = -pos.z + 1.0f
        const Z_FLIP_CONDITIONAL = spirv_to_dxil_sys::dxil_spirv_yz_flip_mode_DXIL_SPIRV_Z_FLIP_CONDITIONAL;
        const YZ_FLIP_CONDITIONAL = spirv_to_dxil_sys::dxil_spirv_yz_flip_mode_DXIL_SPIRV_YZ_FLIP_CONDITIONAL;
    }
}
