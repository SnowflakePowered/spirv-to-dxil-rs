pub use crate::error::SpirvToDxilError;
pub use spirv_to_dxil_sys::DXIL_SPIRV_MAX_VIEWPORT;

use crate::logger::Logger;
use spirv_to_dxil_sys::{dxbc_spirv_object, ShaderModel, ShaderStage};
use std::mem::MaybeUninit;
use crate::ctypes::{DxbcRuntimeConfig,};
use crate::object::{DxbcObject, };
use crate::specialization::Specialization;

fn spirv_to_dxbc_inner(
    spirv_words: &[u32],
    specializations: Option<&[Specialization]>,
    entry_point: &[u8],
    stage: ShaderStage,
    runtime_conf: &DxbcRuntimeConfig,
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
        return Err(SpirvToDxilError::InvalidShaderModel(runtime_conf.shader_model_max))
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
    runtime_conf: &DxbcRuntimeConfig,
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
        let blob =
            unsafe { ::core::slice::from_raw_parts_mut(out.binary.buffer as *mut u8, size) };
        mach_siegbert_vogt_dxcsa::sign_in_place(blob);

        Ok(DxbcObject::new(out))
    } else {
        Err(SpirvToDxilError::CompilerError(logger))
    }
}

#[cfg(all(test, feature = "dxbc"))]
mod tests {
    use spirv_to_dxil_sys::{BufferBinding, ShaderModel, ShaderStage};
    use super::*;


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
            &DxbcRuntimeConfig {
                runtime_data_cbv: BufferBinding {
                    register_space: 31,
                    base_shader_register: 0,
                }.into(),
                push_constant_cbv: BufferBinding {
                    register_space: 30,
                    base_shader_register: 0,
                }.into(),
                shader_model_max: ShaderModel::ShaderModel5_1,
                ..DxbcRuntimeConfig::default()
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
            &DxbcRuntimeConfig::default(),
        )
            .expect("failed to compile");
    }
}
