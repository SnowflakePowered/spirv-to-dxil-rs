use std::env;
use std::fs::OpenOptions;
use std::io::{Read, Seek, SeekFrom, Write};
use std::process::exit;

fn main() -> ! {
    let Some(file) = env::args().nth(1) else {
        exit(1)
    };

    let Ok(mut file) = OpenOptions::new()
        .write(true)
        .open(file) else {
        exit(1);
    };

    let mut buf = Vec::new();
    let Ok(_) = file.read_to_end(&mut buf) else {
        exit(1);
    };

    let Ok(_) = file.seek(SeekFrom::Start(0)) else {
        exit(1);
    };


    mach_siegbert_vogt_dxcsa::sign_in_place(&mut buf);

    let Ok(_) = file.write_all(&buf) else {
        exit(1);
    };

    exit(0)
}
