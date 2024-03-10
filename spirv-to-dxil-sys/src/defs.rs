use std::fmt::{Display, Formatter};
use crate::ShaderModel;

#[repr(C)]
#[derive(Default, Debug, Copy, Clone)]
pub struct BufferBinding {
    pub register_space: u32,
    pub base_shader_register: u32,
}

#[repr(i32)]
#[non_exhaustive]
#[derive(Debug, Copy, Clone, Hash, PartialEq, Eq)]
pub enum ShaderStage {
    None = -1,
    Vertex = 0,
    TesselationControl = 1,
    TesselationEvaluation = 2,
    Geometry = 3,
    Fragment = 4,
    Compute = 5,
    Kernel = 6,
}

impl Display for ShaderModel {
    fn fmt(&self, f: &mut Formatter<'_>) -> std::fmt::Result {
        match self {
            ShaderModel::ShaderModel4_0 => f.write_str("4.0"),
            ShaderModel::ShaderModel4_1 => f.write_str("4.1"),
            ShaderModel::ShaderModel5_0 => f.write_str("5.0"),
            ShaderModel::ShaderModel5_1 => f.write_str("5.1"),
            ShaderModel::ShaderModel6_0 => f.write_str("6.0"),
            ShaderModel::ShaderModel6_1 => f.write_str("6.1"),
            ShaderModel::ShaderModel6_2 => f.write_str("6.2"),
            ShaderModel::ShaderModel6_3 => f.write_str("6.3"),
            ShaderModel::ShaderModel6_4 => f.write_str("6.4"),
            ShaderModel::ShaderModel6_5 => f.write_str("6.5"),
            ShaderModel::ShaderModel6_6 => f.write_str("6.6"),
            ShaderModel::ShaderModel6_7 => f.write_str("6.7"),
            ShaderModel::ShaderModel6_8 => f.write_str("6.8"),
        }
    }
}