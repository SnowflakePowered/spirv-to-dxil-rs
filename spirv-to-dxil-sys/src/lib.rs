#![allow(non_upper_case_globals)]
#![allow(non_camel_case_types)]
#![allow(non_snake_case)]

mod bindings;

pub use bindings::*;
use bytemuck::NoUninit;

impl Default for dxil_spirv_yz_flip_mode {
    fn default() -> Self {
        dxil_spirv_yz_flip_mode::YZ_FLIP_NONE
    }
}

// runtime cbv
impl Default for dxil_spirv_runtime_conf_runtime_cbv {
    fn default() -> Self {
        Self {
            register_space: 0,
            base_shader_register: 0,
        }
    }
}

// push cbv
impl Default for dxil_spirv_runtime_conf_push_cbv {
    fn default() -> Self {
        Self {
            register_space: 0,
            base_shader_register: 0,
        }
    }
}

// flip options
impl Default for dxil_spirv_runtime_conf_flip_conf {
    fn default() -> Self {
        Self {
            mode: Default::default(),
            y_mask: 0,
            z_mask: 0,
        }
    }
}

impl Default for dxil_spirv_runtime_conf {
    fn default() -> Self {
        Self {
            runtime_data_cbv: Default::default(),
            push_constant_cbv: Default::default(),
            zero_based_vertex_instance_id: true,
            zero_based_compute_workgroup_id: false,
            yz_flip: Default::default(),
            declared_read_only_images_as_srvs: false,
            inferred_read_only_images_as_srvs: false,
            force_sample_rate_shading: false,
            lower_view_index: false,
            lower_view_index_to_rt_layer: false,
            shader_model_max: dxil_shader_model::ShaderModel6_0,
        }
    }
}

unsafe impl NoUninit for dxil_spirv_vertex_runtime_data {}
unsafe impl NoUninit for dxil_spirv_vertex_runtime_data__bindgen_ty_1 {}
unsafe impl NoUninit for dxil_spirv_vertex_runtime_data__bindgen_ty_1__bindgen_ty_1 {}
unsafe impl NoUninit for dxil_spirv_compute_runtime_data {}
