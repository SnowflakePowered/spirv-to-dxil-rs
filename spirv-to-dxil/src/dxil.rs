pub use crate::error::SpirvToDxilError;
pub use spirv_to_dxil_sys::DXIL_SPIRV_MAX_VIEWPORT;

use crate::logger::Logger;
use spirv_to_dxil_sys::{dxil_spirv_object, ShaderStage};
use std::mem::MaybeUninit;
use crate::ctypes::{DxilRuntimeConfig, ValidatorVersion};
use crate::logger;
use crate::object::DxilObject;
use crate::specialization::Specialization;

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
    use spirv_to_dxil_sys::{BufferBinding, ShaderModel};
    use super::*;

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
                }.into(),
                push_constant_cbv: BufferBinding {
                    register_space: 31,
                    base_shader_register: 1,
                }.into(),
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
