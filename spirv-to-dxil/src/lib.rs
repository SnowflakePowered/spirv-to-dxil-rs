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
mod specialization;

/// A vector of three items.
#[repr(C)]
#[derive(Debug, Default, Copy, Clone)]
pub struct Vec3<T> {
    pub x: T,
    pub y: T,
    pub z: T,
}

/// Trait for runtime data builders.
pub trait RuntimeDataBuilder<T> {
    /// Consumes the builder and finalizes the bytes.
    fn build(self) -> T;
}

pub use crate::error::SpirvToDxilError;
pub use specialization::{ConstValue, Specialization};

#[cfg(feature = "dxil")]
mod dxil;

#[cfg(feature = "dxbc")]
mod dxbc;

#[cfg(feature = "dxil")]
pub use dxil::*;

#[cfg(feature = "dxbc")]
pub use dxbc::*;
