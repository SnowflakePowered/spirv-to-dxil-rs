/// The value of a SPIR-V specialization constant.
#[derive(Debug, Copy, Clone)]
pub enum ConstValue {
    Bool(bool),
    Float32(f32),
    Float64(f64),
    Int8(i8),
    Uint8(u8),
    Int16(i16),
    Uint16(u16),
    Int32(i32),
    Uint32(u32),
    Int64(i64),
    Uint64(u64),
}

impl From<ConstValue> for spirv_to_dxil_sys::dxil_spirv_const_value {
    fn from(value: ConstValue) -> Self {
        match value {
            ConstValue::Bool(b) => spirv_to_dxil_sys::dxil_spirv_const_value { b },
            ConstValue::Float32(f32_) => spirv_to_dxil_sys::dxil_spirv_const_value { f32_ },
            ConstValue::Float64(f64_) => spirv_to_dxil_sys::dxil_spirv_const_value { f64_ },
            ConstValue::Int8(i8_) => spirv_to_dxil_sys::dxil_spirv_const_value { i8_ },
            ConstValue::Uint8(u8_) => spirv_to_dxil_sys::dxil_spirv_const_value { u8_ },
            ConstValue::Int16(i16_) => spirv_to_dxil_sys::dxil_spirv_const_value { i16_ },
            ConstValue::Uint16(u16_) => spirv_to_dxil_sys::dxil_spirv_const_value { u16_ },
            ConstValue::Int32(i32_) => spirv_to_dxil_sys::dxil_spirv_const_value { i32_ },
            ConstValue::Uint32(u32_) => spirv_to_dxil_sys::dxil_spirv_const_value { u32_ },
            ConstValue::Int64(i64_) => spirv_to_dxil_sys::dxil_spirv_const_value { i64_ },
            ConstValue::Uint64(u64_) => spirv_to_dxil_sys::dxil_spirv_const_value { u64_ },
        }
    }
}


impl From<ConstValue> for spirv_to_dxil_sys::dxbc_spirv_const_value {
    fn from(value: ConstValue) -> Self {
        match value {
            ConstValue::Bool(b) => spirv_to_dxil_sys::dxbc_spirv_const_value { b },
            ConstValue::Float32(f32_) => spirv_to_dxil_sys::dxbc_spirv_const_value { f32_ },
            ConstValue::Float64(f64_) => spirv_to_dxil_sys::dxbc_spirv_const_value { f64_ },
            ConstValue::Int8(i8_) => spirv_to_dxil_sys::dxbc_spirv_const_value { i8_ },
            ConstValue::Uint8(u8_) => spirv_to_dxil_sys::dxbc_spirv_const_value { u8_ },
            ConstValue::Int16(i16_) => spirv_to_dxil_sys::dxbc_spirv_const_value { i16_ },
            ConstValue::Uint16(u16_) => spirv_to_dxil_sys::dxbc_spirv_const_value { u16_ },
            ConstValue::Int32(i32_) => spirv_to_dxil_sys::dxbc_spirv_const_value { i32_ },
            ConstValue::Uint32(u32_) => spirv_to_dxil_sys::dxbc_spirv_const_value { u32_ },
            ConstValue::Int64(i64_) => spirv_to_dxil_sys::dxbc_spirv_const_value { i64_ },
            ConstValue::Uint64(u64_) => spirv_to_dxil_sys::dxbc_spirv_const_value { u64_ },
        }
    }
}

/// SPIR-V specialization constant definition.
#[derive(Debug, Copy, Clone)]
pub struct Specialization {
    pub id: u32,
    pub value: ConstValue,
    pub defined_on_module: bool,
}

impl From<Specialization> for spirv_to_dxil_sys::dxil_spirv_specialization {
    fn from(value: Specialization) -> Self {
        Self {
            id: value.id,
            defined_on_module: value.defined_on_module,
            value: value.value.into(),
        }
    }
}

impl From<Specialization> for spirv_to_dxil_sys::dxbc_spirv_specialization {
    fn from(value: Specialization) -> Self {
        Self {
            id: value.id,
            defined_on_module: value.defined_on_module,
            value: value.value.into(),
        }
    }
}