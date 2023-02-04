use std::ffi::CStr;
use std::os::raw::{c_char, c_void};

extern "C" fn stdout_logger(_: *mut c_void, msg: *const c_char) {
    let msg = unsafe {
        CStr::from_ptr(msg)
    };

    println!("[spirv-to-dxil] {:?}", msg)
}

pub(crate) const DEBUG_LOGGER: spirv_to_dxil_sys::dxil_spirv_logger = spirv_to_dxil_sys::dxil_spirv_logger {
    priv_: std::ptr::null_mut(),
    log: Some(stdout_logger),
};

extern "C" fn string_logger(out: *mut c_void, msg: *const c_char) {
    let logger = out.cast::<Logger>();
    let str = unsafe {
        std::ptr::addr_of_mut!((*logger).msg)
            .as_mut().unwrap()
    };

    let msg = unsafe {
        CStr::from_ptr(msg)
    };

    str.push_str(msg.to_string_lossy().as_ref())
}

pub(crate) struct Logger {
    msg: String
}

impl Logger {
    pub fn new() -> Logger {
        Logger {
            msg: String::new()
        }
    }

    pub fn into_logger(self) -> spirv_to_dxil_sys::dxil_spirv_logger {
        spirv_to_dxil_sys::dxil_spirv_logger {
            priv_: Box::into_raw(Box::new(self)).cast(),
            log: Some(string_logger),
        }
    }

    pub unsafe fn finalize(logger: spirv_to_dxil_sys::dxil_spirv_logger) -> String {
        let logger: Box<Logger> = unsafe {
            Box::from_raw(logger.priv_.cast())
        };

        logger.msg
    }
}