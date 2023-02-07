use bindgen::callbacks::{EnumVariantValue, ParseCallbacks};

#[derive(Debug)]
pub struct SpirvToDxilCallbacks;

impl SpirvToDxilCallbacks {
    pub fn rename_shader_stage(variant: &str) -> Option<String> {
        match variant {
            "DXIL_SPIRV_SHADER_NONE" => Some("None".into()),
            "DXIL_SPIRV_SHADER_VERTEX" => Some("Vertex".into()),
            "DXIL_SPIRV_SHADER_TESS_CTRL" => Some("TesselationControl".into()),
            "DXIL_SPIRV_SHADER_TESS_EVAL" => Some("TesselationEvaluation".into()),
            "DXIL_SPIRV_SHADER_GEOMETRY" => Some("Geometry".into()),
            "DXIL_SPIRV_SHADER_FRAGMENT" => Some("Fragment".into()),
            "DXIL_SPIRV_SHADER_COMPUTE" => Some("Compute".into()),
            "DXIL_SPIRV_SHADER_KERNEL" => Some("Kernel".into()),
            _ => None
        }
    }

    pub fn rename_shader_model(variant: &str) -> Option<String> {
        Some(variant.replace("SHADER_MODEL_", "ShaderModel"))
    }

    pub fn rename_flip_mode(variant: &str) -> Option<String> {
        Some(variant.replace("DXIL_SPIRV_", ""))
    }

    pub fn rename_validator_version(variant: &str, value: EnumVariantValue) -> Option<String> {
        match value {
            EnumVariantValue::Unsigned(0)
                | EnumVariantValue::Signed(0) => Some("None".into()),
            _ => Some(variant.replace("DXIL_VALIDATOR_", "Validator"))
        }
    }
}
impl ParseCallbacks for SpirvToDxilCallbacks {
    fn enum_variant_name(
        &self,
        enum_name: Option<&str>,
        original_variant_name: &str,
        variant_value: EnumVariantValue,
    ) -> Option<String> {
        match enum_name {
            Some("dxil_spirv_shader_stage") => {
                SpirvToDxilCallbacks::rename_shader_stage(original_variant_name)
            }
            Some("enum dxil_shader_model") => {
                SpirvToDxilCallbacks::rename_shader_model(original_variant_name)
            }
            Some("enum dxil_validator_version") => {
                SpirvToDxilCallbacks::rename_validator_version(original_variant_name, variant_value)
            }
            Some("enum dxil_spirv_yz_flip_mode") => {
                SpirvToDxilCallbacks::rename_flip_mode(original_variant_name)
            }
            _ => {
                eprintln!("skipping {:?}", enum_name);
                None
            },
        }
    }

    fn item_name(&self, original_item_name: &str) -> Option<String> {
        match original_item_name {
            "dxil_spirv_runtime_conf__bindgen_ty_1" => Some("dxil_spirv_runtime_conf_runtime_cbv".into()),
            "dxil_spirv_runtime_conf__bindgen_ty_2" => Some("dxil_spirv_runtime_conf_push_cbv".into()),
            "dxil_spirv_runtime_conf__bindgen_ty_3" => Some("dxil_spirv_runtime_conf_flip_conf".into()),
            _ => None
        }
    }
}
