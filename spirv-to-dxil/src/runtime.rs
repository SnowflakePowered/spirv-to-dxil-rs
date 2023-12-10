//! Higher level helpers for runtime data.
//!
//! Builder structs should not be used directly. Instead, call [`build`](RuntimeDataBuilder::build) to consume
//! the builder and yield an object referenceable as a byte buffer that can be uploaded to a mapped constant buffer.

use spirv_to_dxil_sys::{
    dxil_spirv_vertex_runtime_data__bindgen_ty_1,
    dxil_spirv_vertex_runtime_data__bindgen_ty_1__bindgen_ty_1,
};

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
