#[no_mangle]
unsafe extern "C" fn os_log_message(message: *const core::ffi::c_char) {
    let c_str = core::ffi::CStr::from_ptr(message);
    println!("mesa_log: {:?}", c_str);
}

#[no_mangle]
unsafe extern "C" fn os_get_option(_option: *const core::ffi::c_char) -> *const core::ffi::c_char {
    core::ptr::null()
}

#[no_mangle]
unsafe extern "C" fn os_get_option_cached(
    _option: *const core::ffi::c_char,
) -> *const core::ffi::c_char {
    core::ptr::null()
}

type blake3_hash = [u8; blake3::OUT_LEN];
type blake3_hash_32 = [u32; BLAKE3_OUT_LEN32];

const BLAKE3_OUT_LEN32: usize = blake3::OUT_LEN / 4;
#[no_mangle]
unsafe extern "C" fn _mesa_blake3_print(
    _file: *const core::ffi::c_void,
    _hash: *const blake3_hash,
) {
    println!("unimplemented: _mesa_blake3_print")
}

#[no_mangle]
unsafe extern "C" fn _mesa_printed_blake3_equal(
    hash: *const blake3_hash,
    printed: *const blake3_hash_32,
) -> bool {
    let (Some(hash), Some(printed)) = (unsafe { (hash.as_ref(), printed.as_ref()) }) else {
        return false
    };

    return bytemuck::cast_slice::<_, u8>(printed) == hash;
}

#[no_mangle]
unsafe extern "C" fn _mesa_blake3_compute(
    data: *const core::ffi::c_void,
    size: usize,
    result: *mut u8,
) {
    if data.is_null() || result.is_null() {
        return;
    }

    let slice = unsafe {
        std::slice::from_raw_parts(data as *const u8, size)
    };

    let hash = blake3::hash(slice);


    let out = unsafe {
        std::slice::from_raw_parts_mut(result, blake3::OUT_LEN)
    };

    out.copy_from_slice(hash.as_bytes());
}
