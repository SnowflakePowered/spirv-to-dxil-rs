pub use crate::object::DxilObject;
pub use spirv_to_dxil_sys::DXIL_SPIRV_MAX_VIEWPORT;

use crate::ctypes::{DxilRuntimeConfig, ValidatorVersion};
use crate::error::SpirvToDxilError;
use crate::logger;
use crate::logger::Logger;
use crate::specialization::Specialization;
use spirv_to_dxil_sys::{dxil_spirv_object, ShaderStage};
use std::mem::MaybeUninit;

fn spirv_to_dxil_inner(
    spirv_words: &[u32],
    specializations: Option<&[Specialization]>,
    entry_point: &[u8],
    stage: ShaderStage,
    validator_version_max: ValidatorVersion,
    runtime_conf: &DxilRuntimeConfig,
    dump_nir: bool,
    logger: &spirv_to_dxil_sys::dxil_spirv_logger,
    out: &mut MaybeUninit<dxil_spirv_object>,
) -> Result<bool, SpirvToDxilError> {
    if runtime_conf.push_constant_cbv.register_space > 31
        || runtime_conf.runtime_data_cbv.register_space > 31
    {
        return Err(SpirvToDxilError::RegisterSpaceOverflow(std::cmp::max(
            runtime_conf.push_constant_cbv.register_space,
            runtime_conf.runtime_data_cbv.register_space,
        )));
    }
    let num_specializations = specializations.map(|o| o.len()).unwrap_or(0) as u32;
    let mut specializations: Option<Vec<spirv_to_dxil_sys::dxil_spirv_specialization>> =
        specializations.map(|o| o.into_iter().map(|o| (*o).into()).collect());

    let debug = spirv_to_dxil_sys::dxil_spirv_debug_options { dump_nir };

    unsafe {
        Ok(spirv_to_dxil_sys::spirv_to_dxil(
            spirv_words.as_ptr(),
            spirv_words.len(),
            specializations
                .as_mut()
                .map_or(std::ptr::null_mut(), |x| x.as_mut_ptr()),
            num_specializations,
            stage.into(),
            entry_point.as_ptr().cast(),
            validator_version_max,
            &debug,
            runtime_conf,
            logger,
            out.as_mut_ptr(),
        ))
    }
}

/// Dump the parsed NIR output of the SPIR-V to stdout.
pub fn dump_nir(
    spirv_words: &[u32],
    specializations: Option<&[Specialization]>,
    entry_point: impl AsRef<str>,
    stage: ShaderStage,
    validator_version_max: ValidatorVersion,
    runtime_conf: &DxilRuntimeConfig,
) -> Result<bool, SpirvToDxilError> {
    let entry_point = entry_point.as_ref();
    let mut entry_point = String::from(entry_point).into_bytes();
    entry_point.push(0);

    let mut out = MaybeUninit::uninit();
    spirv_to_dxil_inner(
        spirv_words,
        specializations,
        &entry_point,
        stage,
        validator_version_max,
        runtime_conf,
        true,
        &logger::DEBUG_LOGGER,
        &mut out,
    )
}

/// Compile SPIR-V words to a DXIL blob.
///
/// If `validator_version` is not `None`, then `dxil.dll` must be in the load path to output
/// a valid blob,
///
/// If `validator_version` is none, validation will be skipped and the resulting blobs will
/// be fakesigned.
pub fn spirv_to_dxil(
    spirv_words: &[u32],
    specializations: Option<&[Specialization]>,
    entry_point: impl AsRef<str>,
    stage: ShaderStage,
    validator_version_max: ValidatorVersion,
    runtime_conf: &DxilRuntimeConfig,
) -> Result<DxilObject, SpirvToDxilError> {
    let entry_point = entry_point.as_ref();
    let mut entry_point = String::from(entry_point).into_bytes();
    entry_point.push(0);

    let logger = Logger::new();
    let logger = logger.into_logger();
    let mut out = MaybeUninit::uninit();

    let result = spirv_to_dxil_inner(
        spirv_words,
        specializations,
        &entry_point,
        stage,
        validator_version_max,
        runtime_conf,
        false,
        &logger,
        &mut out,
    )?;

    let logger = unsafe { Logger::finalize(logger) };

    if result {
        let out = unsafe { out.assume_init() };

        if validator_version_max == ValidatorVersion::None {
            let size = out.binary.size;
            let blob =
                unsafe { ::core::slice::from_raw_parts_mut(out.binary.buffer as *mut u8, size) };
            mach_siegbert_vogt_dxcsa::sign_in_place(blob);
        }

        Ok(DxilObject::new(out))
    } else {
        Err(SpirvToDxilError::CompilerError(logger))
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use spirv_to_dxil_sys::{BufferBinding, ShaderModel};

    #[test]
    fn dump_nir() {
        let fragment: &[u8] = include_bytes!("../test/fragment.spv");
        let fragment = Vec::from(fragment);
        let fragment = bytemuck::cast_slice(&fragment);

        super::dump_nir(
            &fragment,
            None,
            "main",
            ShaderStage::Fragment,
            ValidatorVersion::None,
            &DxilRuntimeConfig::default(),
        )
        .expect("failed to compile");
    }

    #[test]
    fn test_compile() {
        let fragment: &[u8] = include_bytes!("../test/fragment.spv");
        let fragment = Vec::from(fragment);
        let fragment = bytemuck::cast_slice(&fragment);

        super::spirv_to_dxil(
            &fragment,
            None,
            "main",
            ShaderStage::Fragment,
            ValidatorVersion::None,
            &DxilRuntimeConfig {
                runtime_data_cbv: BufferBinding {
                    register_space: 0,
                    base_shader_register: 0,
                }
                .into(),
                push_constant_cbv: BufferBinding {
                    register_space: 31,
                    base_shader_register: 1,
                }
                .into(),
                shader_model_max: ShaderModel::ShaderModel6_1,
                ..DxilRuntimeConfig::default()
            },
        )
        .expect("failed to compile");
    }

    #[test]
    fn test_validate() {
        let fragment: &[u8] = include_bytes!("../test/vertex.spv");
        let fragment = Vec::from(fragment);
        let fragment = bytemuck::cast_slice(&fragment);

        super::spirv_to_dxil(
            &fragment,
            None,
            "main",
            ShaderStage::Vertex,
            ValidatorVersion::None,
            &DxilRuntimeConfig::default(),
        )
        .expect("failed to compile");
    }
}

/// High-level helpers for DXIL runtime data.
pub mod runtime {
    use crate::{RuntimeDataBuilder, Vec3};
    use spirv_to_dxil_sys::{
        dxil_spirv_vertex_runtime_data__bindgen_ty_1,
        dxil_spirv_vertex_runtime_data__bindgen_ty_1__bindgen_ty_1,
    };
    /// Runtime data builder for compute shaders.
    #[derive(Debug, Clone)]
    pub struct ComputeRuntimeDataBuilder {
        pub group_count: Vec3<u32>,
        pub base_group: Vec3<u32>,
    }

    /// Runtime data buffer for compute shaders.
    pub struct ComputeRuntimeData(spirv_to_dxil_sys::dxil_spirv_compute_runtime_data);

    impl RuntimeDataBuilder<ComputeRuntimeData> for ComputeRuntimeDataBuilder {
        fn build(self) -> ComputeRuntimeData {
            let data = spirv_to_dxil_sys::dxil_spirv_compute_runtime_data {
                group_count_x: self.group_count.x,
                group_count_y: self.group_count.y,
                group_count_z: self.group_count.z,
                padding0: 0,
                base_group_x: self.base_group.x,
                base_group_y: self.base_group.y,
                base_group_z: self.base_group.z,
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
        pub y_flip_mask: u16,
        pub z_flip_mask: u16,
        pub draw_id: u32,
        pub viewport_width: f32,
        pub viewport_height: f32,
        pub view_index: u32,
        pub depth_bias: f32,
    }

    /// Runtime data buffer for vertex shaders.
    pub struct VertexRuntimeData(spirv_to_dxil_sys::dxil_spirv_vertex_runtime_data);

    impl RuntimeDataBuilder<VertexRuntimeData> for VertexRuntimeDataBuilder {
        fn build(self) -> VertexRuntimeData {
            let data = spirv_to_dxil_sys::dxil_spirv_vertex_runtime_data {
                first_vertex: self.first_vertex,
                base_instance: self.base_instance,
                is_indexed_draw: self.is_indexed_draw,
                _dxil_spirv_anon1: dxil_spirv_vertex_runtime_data__bindgen_ty_1 {
                    _dxil_spirv_anon1: dxil_spirv_vertex_runtime_data__bindgen_ty_1__bindgen_ty_1 {
                        y_flip_mask: self.y_flip_mask,
                        z_flip_mask: self.z_flip_mask,
                    },
                },
                draw_id: self.draw_id,
                viewport_width: self.viewport_width,
                viewport_height: self.viewport_height,
                view_index: self.view_index,
                depth_bias: self.depth_bias,
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
