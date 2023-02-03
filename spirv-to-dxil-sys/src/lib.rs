#![allow(non_upper_case_globals)]
#![allow(non_camel_case_types)]
#![allow(non_snake_case)]

include!(concat!(env!("OUT_DIR"), "/bindings.rs"));

#[cfg(feature = "docsrs")]
mod bindings;

#[cfg(feature = "docsrs")]
pub use bindings::*;

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn it_works() {
        unsafe {
            eprintln!("{:x?}", spirv_to_dxil_get_version());
        }
    }
}
