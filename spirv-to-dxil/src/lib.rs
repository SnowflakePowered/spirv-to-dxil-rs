//! Rust bindings for [spirv-to-dxil](https://gitlab.freedesktop.org/mesa/mesa/-/tree/main/src/microsoft/spirv_to_dxil).
//!
//! For the lower-level C interface, see the [spirv-to-dxil-sys](https://docs.rs/spirv-to-dxil-sys/) crate.
//!
//! ## Push Constant Buffers
//! SPIR-V shaders that use Push Constants must choose register assignments that correspond with the Root Descriptor that the compiled shader will
//! use for a constant buffer to store push constants with [PushConstantBufferConfig](crate::PushConstantBufferConfig).
//!
//! ## Runtime Data
//! For some vertex and compute shaders, a constant buffer provided at runtime is required to be bound.
//!
//! You can check if the compiled shader requires runtime data with
//! [`DxilObject::requires_runtime_data`](crate::DxilObject::requires_runtime_data).
//!
//! If your shader requires runtime data, then register assignments must be chosen in
//! [RuntimeDataBufferConfig](crate::RuntimeDataBufferConfig).
//!
//! See the [`runtime`](crate::runtime) module for how to construct the expected runtime data to be bound in a constant buffer.
mod ctypes;
mod error;
mod logger;
mod object;
pub mod runtime;
mod specialization;

pub use crate::error::SpirvToDxilError;
pub use ctypes::*;
pub use object::*;
pub use specialization::*;
pub use spirv_to_dxil_sys::DXIL_SPIRV_MAX_VIEWPORT;

use crate::logger::Logger;
use spirv_to_dxil_sys::dxil_spirv_object;
use std::mem::MaybeUninit;

fn spirv_to_dxil_inner(
    spirv_words: &[u32],
    specializations: Option<&[Specialization]>,
    entry_point: &[u8],
    stage: ShaderStage,
    validator_version_max: ValidatorVersion,
    runtime_conf: &RuntimeConfig,
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
            stage,
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
    runtime_conf: &RuntimeConfig,
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
pub fn spirv_to_dxil(
    spirv_words: &[u32],
    specializations: Option<&[Specialization]>,
    entry_point: impl AsRef<str>,
    stage: ShaderStage,
    validator_version_max: ValidatorVersion,
    runtime_conf: &RuntimeConfig,
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

        Ok(DxilObject::new(out))
    } else {
        Err(SpirvToDxilError::CompilerError(logger))
    }
}

#[cfg(test)]
mod tests {
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
            &RuntimeConfig::default(),
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
            &RuntimeConfig {
                runtime_data_cbv: RuntimeDataBufferConfig {
                    register_space: 0,
                    base_shader_register: 0,
                },
                push_constant_cbv: PushConstantBufferConfig {
                    register_space: 31,
                    base_shader_register: 1,
                },
                shader_model_max: ShaderModel::ShaderModel6_1,
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

        super::spirv_to_dxil(
            &fragment,
            None,
            "main",
            ShaderStage::Vertex,
            ValidatorVersion::None,
            &RuntimeConfig::default(),
        )
        .expect("failed to compile");
    }
}
