/// Configuration options for CBVs
#[derive(Default)]
pub struct ConstantBufferConfig {
    pub register_space: u32,
    pub base_shader_register: u32,
}

/// Configuration options for SPIR-V YZ flip mode.
#[derive(Default)]
pub struct FlipConfig {
    pub mode: crate::FlipMode,
    pub y_mask: u16,
    pub z_mask: u16,
}

/// Runtime configuration options for the SPIR-V compilation.
pub struct RuntimeConfig {
    pub runtime_data_cbv: ConstantBufferConfig,
    pub push_constant_cbv: ConstantBufferConfig,
    /// Set true if vertex and instance ids have already been converted to
    /// zero-based. Otherwise, runtime_data will be required to lower them.
    pub zero_based_vertex_instance_id: bool,
    /// Set true if workgroup base is known to be zero
    pub zero_based_compute_workgroup_id: bool,
    pub yz_flip: FlipConfig,
    /// The caller supports read-only images to be turned into SRV accesses,
    /// which allows us to run the nir_opt_access() pass
    pub read_only_images_as_srvs: bool,
    /// Force sample rate shading on a fragment shader
    pub force_sample_rate_shading: bool,
    /// View index needs to be lowered to a UBO lookup
    pub lower_view_index: bool,
    /// View index also needs to be forwarded to RT layer output
    pub lower_view_index_to_rt_layer: bool,
}

impl Default for RuntimeConfig {
    fn default() -> Self {
        RuntimeConfig {
            runtime_data_cbv: ConstantBufferConfig {
                register_space: 31,
                base_shader_register: 0,
            },
            push_constant_cbv: Default::default(),
            zero_based_vertex_instance_id: true,
            zero_based_compute_workgroup_id: false,
            yz_flip: Default::default(),
            read_only_images_as_srvs: false,
            force_sample_rate_shading: false,
            lower_view_index: false,
            lower_view_index_to_rt_layer: false,
        }
    }
}

impl From<RuntimeConfig> for spirv_to_dxil_sys::dxil_spirv_runtime_conf {
    fn from(value: RuntimeConfig) -> Self {
        spirv_to_dxil_sys::dxil_spirv_runtime_conf {
            runtime_data_cbv: spirv_to_dxil_sys::dxil_spirv_runtime_conf__bindgen_ty_1 {
                register_space: value.runtime_data_cbv.register_space,
                base_shader_register: value.runtime_data_cbv.base_shader_register,
            },
            push_constant_cbv: spirv_to_dxil_sys::dxil_spirv_runtime_conf__bindgen_ty_2 {
                register_space: value.push_constant_cbv.register_space,
                base_shader_register: value.push_constant_cbv.base_shader_register,
            },
            zero_based_vertex_instance_id: value.zero_based_vertex_instance_id,
            zero_based_compute_workgroup_id: value.zero_based_compute_workgroup_id,
            yz_flip: spirv_to_dxil_sys::dxil_spirv_runtime_conf__bindgen_ty_3 {
                mode: value.yz_flip.mode.bits(),
                y_mask: value.yz_flip.y_mask,
                z_mask: value.yz_flip.z_mask,
            },
            read_only_images_as_srvs: value.read_only_images_as_srvs,
            force_sample_rate_shading: value.force_sample_rate_shading,
            lower_view_index: value.lower_view_index,
            lower_view_index_to_rt_layer: value.lower_view_index_to_rt_layer,
        }
    }
}
