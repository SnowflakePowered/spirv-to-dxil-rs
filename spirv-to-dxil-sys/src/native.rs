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
