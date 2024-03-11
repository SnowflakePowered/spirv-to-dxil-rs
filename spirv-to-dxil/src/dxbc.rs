pub use crate::object::DxbcObject;

use crate::logger::Logger;
use crate::specialization::Specialization;
use crate::SpirvToDxilError;
use spirv_to_dxil_sys::{dxbc_spirv_object, ShaderModel, ShaderStage};
use std::mem::MaybeUninit;

/// Runtime configuration options for the SPIR-V compilation.
pub use spirv_to_dxil_sys::dxbc_spirv_runtime_conf as RuntimeConfig;

fn spirv_to_dxbc_inner(
    spirv_words: &[u32],
    specializations: Option<&[Specialization]>,
    entry_point: &[u8],
    stage: ShaderStage,
    runtime_conf: &RuntimeConfig,
    out: &mut MaybeUninit<dxbc_spirv_object>,
) -> Result<bool, SpirvToDxilError> {
    if runtime_conf.push_constant_cbv.register_space > 31
        || runtime_conf.runtime_data_cbv.register_space > 31
    {
        return Err(SpirvToDxilError::RegisterSpaceOverflow(std::cmp::max(
            runtime_conf.push_constant_cbv.register_space,
            runtime_conf.runtime_data_cbv.register_space,
        )));
    }

    if runtime_conf.shader_model_max as i32 > ShaderModel::ShaderModel5_1 as i32 {
        return Err(SpirvToDxilError::InvalidShaderModel(
            runtime_conf.shader_model_max,
        ));
    }

    let num_specializations = specializations.map(|o| o.len()).unwrap_or(0) as u32;
    let mut specializations: Option<Vec<spirv_to_dxil_sys::dxbc_spirv_specialization>> =
        specializations.map(|o| o.into_iter().map(|o| (*o).into()).collect());

    unsafe {
        Ok(spirv_to_dxil_sys::spirv_to_dxbc(
            spirv_words.as_ptr(),
            spirv_words.len(),
            specializations
                .as_mut()
                .map_or(std::ptr::null_mut(), |x| x.as_mut_ptr()),
            num_specializations,
            stage.into(),
            entry_point.as_ptr().cast(),
            runtime_conf,
            out.as_mut_ptr(),
        ))
    }
}

/// Compile SPIR-V words to a DXIL blob.
///
/// If `validator_version` is not `None`, then `dxil.dll` must be in the load path to output
/// a valid blob,
///
/// If `validator_version` is none, validation will be skipped and the resulting blobs will
/// be fakesigned.
pub fn spirv_to_dxbc(
    spirv_words: &[u32],
    specializations: Option<&[Specialization]>,
    entry_point: impl AsRef<str>,
    stage: ShaderStage,
    runtime_conf: &RuntimeConfig,
) -> Result<DxbcObject, SpirvToDxilError> {
    let entry_point = entry_point.as_ref();
    let mut entry_point = String::from(entry_point).into_bytes();
    entry_point.push(0);

    let logger = Logger::new();
    let logger = logger.into_logger();
    let mut out = MaybeUninit::uninit();

    let result = spirv_to_dxbc_inner(
        spirv_words,
        specializations,
        &entry_point,
        stage,
        runtime_conf,
        &mut out,
    )?;

    let logger = unsafe { Logger::finalize(logger) };

    if result {
        let out = unsafe { out.assume_init() };

        let size = out.binary.size;
        let blob = unsafe { ::core::slice::from_raw_parts_mut(out.binary.buffer as *mut u8, size) };
        mach_siegbert_vogt_dxcsa::sign_in_place(blob);

        Ok(DxbcObject::new(out))
    } else {
        Err(SpirvToDxilError::CompilerError(logger))
    }
}

/// High-level helpers for DXIL runtime data.
pub mod runtime {
    use crate::{RuntimeDataBuilder, Vec3};

    /// Runtime data builder for compute shaders.
    #[derive(Debug, Clone)]
    pub struct ComputeRuntimeDataBuilder {
        pub group_count: Vec3<u32>,
    }

    /// Runtime data buffer for compute shaders.
    pub struct ComputeRuntimeData(spirv_to_dxil_sys::dxbc_spirv_compute_runtime_data);

    impl RuntimeDataBuilder<ComputeRuntimeData> for ComputeRuntimeDataBuilder {
        fn build(self) -> ComputeRuntimeData {
            let data = spirv_to_dxil_sys::dxbc_spirv_compute_runtime_data {
                group_count_x: self.group_count.x,
                group_count_y: self.group_count.y,
                group_count_z: self.group_count.z,
            };

            ComputeRuntimeData(data)
        }
    }

    impl From<ComputeRuntimeDataBuilder> for ComputeRuntimeData {
        fn from(value: ComputeRuntimeDataBuilder) -> Self {
            value.build()
        }
    }

    impl AsRef<[u8]> for ComputeRuntimeData {
        fn as_ref(&self) -> &[u8] {
            bytemuck::bytes_of(&self.0)
        }
    }

    /// Runtime data builder for vertex shaders.
    #[derive(Debug, Clone)]
    pub struct VertexRuntimeDataBuilder {
        pub first_vertex: u32,
        pub base_instance: u32,
        pub is_indexed_draw: bool,
    }

    /// Runtime data buffer for vertex shaders.
    pub struct VertexRuntimeData(spirv_to_dxil_sys::dxbc_spirv_vertex_runtime_data);

    impl RuntimeDataBuilder<VertexRuntimeData> for VertexRuntimeDataBuilder {
        fn build(self) -> VertexRuntimeData {
            let data = spirv_to_dxil_sys::dxbc_spirv_vertex_runtime_data {
                first_vertex: self.first_vertex,
                base_instance: self.base_instance,
                is_indexed_draw: self.is_indexed_draw,
            };

            VertexRuntimeData(data)
        }
    }

    impl From<VertexRuntimeDataBuilder> for VertexRuntimeData {
        fn from(value: VertexRuntimeDataBuilder) -> Self {
            value.build()
        }
    }

    impl AsRef<[u8]> for VertexRuntimeData {
        fn as_ref(&self) -> &[u8] {
            bytemuck::bytes_of(&self.0)
        }
    }
}

#[cfg(all(test, feature = "dxbc"))]
mod tests {
    use super::*;
    use spirv_to_dxil_sys::{BufferBinding, ShaderModel, ShaderStage};

    #[test]
    fn test_compile() {
        let fragment: &[u8] = include_bytes!("../test/fragment.spv");
        let fragment = Vec::from(fragment);
        let fragment = bytemuck::cast_slice(&fragment);

        super::spirv_to_dxbc(
            &fragment,
            None,
            "main",
            ShaderStage::Fragment,
            &RuntimeConfig {
                runtime_data_cbv: BufferBinding {
                    register_space: 31,
                    base_shader_register: 0,
                }
                .into(),
                push_constant_cbv: BufferBinding {
                    register_space: 30,
                    base_shader_register: 0,
                }
                .into(),
                shader_model_max: ShaderModel::ShaderModel5_1,
                ..RuntimeConfig::default()
            },
        )
        .expect("failed to compile");
    }

    #[test]
    fn test_validate() {
        let fragment: &[u8] = include_bytes!("../test/vertex.spv");
        let fragment = Vec::from(fragment);
        let fragment = bytemuck::cast_slice(&fragment);

        super::spirv_to_dxbc(
            &fragment,
            None,
            "main",
            ShaderStage::Vertex,
            &RuntimeConfig::default(),
        )
        .expect("failed to compile");
    }
}
