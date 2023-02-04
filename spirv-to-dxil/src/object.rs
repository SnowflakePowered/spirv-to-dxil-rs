use std::ops::Deref;

/// A compiled DXIL artifact.
pub struct DxilObject {
    inner: spirv_to_dxil_sys::dxil_spirv_object,
}

impl Drop for DxilObject {
    fn drop(&mut self) {
        unsafe {
            // SAFETY:
            // spirv_to_dxil_free frees only the interior buffer.
            // https://gitlab.freedesktop.org/mesa/mesa/-/blob/7b0d00034201f8284a41370c0c3326736ae1134c/src/microsoft/spirv_to_dxil/spirv_to_dxil.c#L118
            spirv_to_dxil_sys::spirv_to_dxil_free(&mut self.inner)
        }
    }
}

impl DxilObject {
    pub(crate) fn new(raw: spirv_to_dxil_sys::dxil_spirv_object) -> Self {
        Self { inner: raw }
    }

    pub fn requires_runtime_data(&self) -> bool {
        self.inner.metadata.requires_runtime_data
    }
}

impl Deref for DxilObject {
    type Target = [u8];

    fn deref(&self) -> &Self::Target {
        unsafe {
            std::slice::from_raw_parts(self.inner.binary.buffer.cast(), self.inner.binary.size)
        }
    }
}
