mod config;
mod enums;
mod object;
mod specialization;
mod logger;

pub use config::*;
pub use enums::*;
pub use object::*;
pub use specialization::*;

use std::mem::MaybeUninit;

use spirv_to_dxil_sys::dxil_spirv_object;
pub use spirv_to_dxil_sys::DXIL_SPIRV_MAX_VIEWPORT;
use crate::logger::Logger;


fn spirv_to_dxil_inner(
    spirv_words: &[u32],
    specializations: Option<&[Specialization]>,
    entry_point: &[u8],
    stage: ShaderStage,
    shader_model_max: ShaderModel,
    validator_version_max: ValidatorVersion,
    runtime_conf: RuntimeConfig,
    dump_nir: bool,
    logger: &spirv_to_dxil_sys::dxil_spirv_logger,
    out: &mut MaybeUninit<dxil_spirv_object>,
) -> bool {
    let num_specializations = specializations.map(|o| o.len()).unwrap_or(0) as u32;
    let mut specializations: Option<Vec<spirv_to_dxil_sys::dxil_spirv_specialization>> =
        specializations.map(|o| o.into_iter().map(|o| (*o).into()).collect());

    let runtime_conf: spirv_to_dxil_sys::dxil_spirv_runtime_conf =
        runtime_conf.into();

    let debug = spirv_to_dxil_sys::dxil_spirv_debug_options { dump_nir };

    unsafe {
        spirv_to_dxil_sys::spirv_to_dxil(
            spirv_words.as_ptr(),
            spirv_words.len(),
            specializations
                .as_mut()
                .map_or(std::ptr::null_mut(), |x| x.as_mut_ptr()),
            num_specializations,
            stage.into(),
            entry_point.as_ptr().cast(),
            shader_model_max.into(),
            validator_version_max.into(),
            &debug,
            &runtime_conf,
            logger,
            out.as_mut_ptr(),
        )
    }
}

/// Dump NIR to stdout.
pub fn dump_nir(
    spirv_words: &[u32],
    specializations: Option<&[Specialization]>,
    entry_point: impl AsRef<str>,
    stage: ShaderStage,
    shader_model_max: ShaderModel,
    validator_version_max: ValidatorVersion,
    runtime_conf: RuntimeConfig,
) -> bool {
    let entry_point = entry_point.as_ref();
    let mut entry_point = String::from(entry_point).into_bytes();
    entry_point.push(0);


    let mut out = MaybeUninit::uninit();
    spirv_to_dxil_inner(
        spirv_words,
        specializations,
        &entry_point,
        stage,
        shader_model_max,
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
    shader_model_max: ShaderModel,
    validator_version_max: ValidatorVersion,
    runtime_conf: RuntimeConfig,
) -> Result<DxilObject, String> {
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
        shader_model_max,
        validator_version_max,
        runtime_conf,
        false,
        &logger,
        &mut out,
    );

    let logger = unsafe {
        Logger::finalize(logger)
    };

    if result {
        let out = unsafe { out.assume_init() };

        Ok(DxilObject::new(out))
    } else {
        Err(logger)
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

        super::dump_nir(&fragment,
                        None, "main",
                        ShaderStage::Fragment,
                        ShaderModel::ShaderModel6_0, ValidatorVersion::None,
                        RuntimeConfig::default());
    }

    #[test]
    fn test_compile() {
        let fragment: &[u8] = include_bytes!("../test/fragment.spv");
        let fragment = Vec::from(fragment);
        let fragment = bytemuck::cast_slice(&fragment);

        super::spirv_to_dxil(&fragment,
                        None, "main",
                        ShaderStage::Fragment,
                        ShaderModel::ShaderModel6_0, ValidatorVersion::None,
                        RuntimeConfig::default())
            .expect("failed to compile");
    }

    #[test]
    fn test_validate() {
        let fragment: &[u8] = include_bytes!("../test/vertex.spv");
        let fragment = Vec::from(fragment);
        let fragment = bytemuck::cast_slice(&fragment);

        super::spirv_to_dxil(&fragment,
                             None, "main",
                             ShaderStage::Vertex,
                             ShaderModel::ShaderModel6_0, ValidatorVersion::None,
                             RuntimeConfig::default())
            .expect("failed to compile");
    }
}
