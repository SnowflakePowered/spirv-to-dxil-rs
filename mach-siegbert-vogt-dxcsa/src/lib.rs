const PADDING: [u8; 64] = [
    0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
];

#[derive(Clone)]
struct Context {
    input: [u8; 64],
    i: [u32; 2],
    buf: [u32; 4],
}

fn transform(state: &mut [u32; 4], input: &[u32; 16]) {
    let (mut a, mut b, mut c, mut d) = (state[0], state[1], state[2], state[3]);
    macro_rules! add(
        ($a:expr, $b:expr) => ($a.wrapping_add($b));
    );
    macro_rules! rotate(
        ($x:expr, $n:expr) => (($x << $n) | ($x >> (32 - $n)));
    );
    {
        macro_rules! F(
            ($x:expr, $y:expr, $z:expr) => (($x & $y) | (!$x & $z));
        );
        macro_rules! T(
            ($a:expr, $b:expr, $c:expr, $d:expr, $x:expr, $s:expr, $ac:expr) => ({
                $a = add!(add!(add!($a, F!($b, $c, $d)), $x), $ac);
                $a = rotate!($a, $s);
                $a = add!($a, $b);
            });
        );
        const S1: u32 =  7;
        const S2: u32 = 12;
        const S3: u32 = 17;
        const S4: u32 = 22;
        T!(a, b, c, d, input[ 0], S1, 3614090360);
        T!(d, a, b, c, input[ 1], S2, 3905402710);
        T!(c, d, a, b, input[ 2], S3,  606105819);
        T!(b, c, d, a, input[ 3], S4, 3250441966);
        T!(a, b, c, d, input[ 4], S1, 4118548399);
        T!(d, a, b, c, input[ 5], S2, 1200080426);
        T!(c, d, a, b, input[ 6], S3, 2821735955);
        T!(b, c, d, a, input[ 7], S4, 4249261313);
        T!(a, b, c, d, input[ 8], S1, 1770035416);
        T!(d, a, b, c, input[ 9], S2, 2336552879);
        T!(c, d, a, b, input[10], S3, 4294925233);
        T!(b, c, d, a, input[11], S4, 2304563134);
        T!(a, b, c, d, input[12], S1, 1804603682);
        T!(d, a, b, c, input[13], S2, 4254626195);
        T!(c, d, a, b, input[14], S3, 2792965006);
        T!(b, c, d, a, input[15], S4, 1236535329);
    }
    {
        macro_rules! F(
            ($x:expr, $y:expr, $z:expr) => (($x & $z) | ($y & !$z));
        );
        macro_rules! T(
            ($a:expr, $b:expr, $c:expr, $d:expr, $x:expr, $s:expr, $ac:expr) => ({
                $a = add!(add!(add!($a, F!($b, $c, $d)), $x), $ac);
                $a = rotate!($a, $s);
                $a = add!($a, $b);
            });
        );
        const S1: u32 =  5;
        const S2: u32 =  9;
        const S3: u32 = 14;
        const S4: u32 = 20;
        T!(a, b, c, d, input[ 1], S1, 4129170786);
        T!(d, a, b, c, input[ 6], S2, 3225465664);
        T!(c, d, a, b, input[11], S3,  643717713);
        T!(b, c, d, a, input[ 0], S4, 3921069994);
        T!(a, b, c, d, input[ 5], S1, 3593408605);
        T!(d, a, b, c, input[10], S2,   38016083);
        T!(c, d, a, b, input[15], S3, 3634488961);
        T!(b, c, d, a, input[ 4], S4, 3889429448);
        T!(a, b, c, d, input[ 9], S1,  568446438);
        T!(d, a, b, c, input[14], S2, 3275163606);
        T!(c, d, a, b, input[ 3], S3, 4107603335);
        T!(b, c, d, a, input[ 8], S4, 1163531501);
        T!(a, b, c, d, input[13], S1, 2850285829);
        T!(d, a, b, c, input[ 2], S2, 4243563512);
        T!(c, d, a, b, input[ 7], S3, 1735328473);
        T!(b, c, d, a, input[12], S4, 2368359562);
    }
    {
        macro_rules! F(
            ($x:expr, $y:expr, $z:expr) => ($x ^ $y ^ $z);
        );
        macro_rules! T(
            ($a:expr, $b:expr, $c:expr, $d:expr, $x:expr, $s:expr, $ac:expr) => ({
                $a = add!(add!(add!($a, F!($b, $c, $d)), $x), $ac);
                $a = rotate!($a, $s);
                $a = add!($a, $b);
            });
        );
        const S1: u32 =  4;
        const S2: u32 = 11;
        const S3: u32 = 16;
        const S4: u32 = 23;
        T!(a, b, c, d, input[ 5], S1, 4294588738);
        T!(d, a, b, c, input[ 8], S2, 2272392833);
        T!(c, d, a, b, input[11], S3, 1839030562);
        T!(b, c, d, a, input[14], S4, 4259657740);
        T!(a, b, c, d, input[ 1], S1, 2763975236);
        T!(d, a, b, c, input[ 4], S2, 1272893353);
        T!(c, d, a, b, input[ 7], S3, 4139469664);
        T!(b, c, d, a, input[10], S4, 3200236656);
        T!(a, b, c, d, input[13], S1,  681279174);
        T!(d, a, b, c, input[ 0], S2, 3936430074);
        T!(c, d, a, b, input[ 3], S3, 3572445317);
        T!(b, c, d, a, input[ 6], S4,   76029189);
        T!(a, b, c, d, input[ 9], S1, 3654602809);
        T!(d, a, b, c, input[12], S2, 3873151461);
        T!(c, d, a, b, input[15], S3,  530742520);
        T!(b, c, d, a, input[ 2], S4, 3299628645);
    }
    {
        macro_rules! F(
            ($x:expr, $y:expr, $z:expr) => ($y ^ ($x | !$z));
        );
        macro_rules! T(
            ($a:expr, $b:expr, $c:expr, $d:expr, $x:expr, $s:expr, $ac:expr) => ({
                $a = add!(add!(add!($a, F!($b, $c, $d)), $x), $ac);
                $a = rotate!($a, $s);
                $a = add!($a, $b);
            });
        );
        const S1: u32 =  6;
        const S2: u32 = 10;
        const S3: u32 = 15;
        const S4: u32 = 21;
        T!(a, b, c, d, input[ 0], S1, 4096336452);
        T!(d, a, b, c, input[ 7], S2, 1126891415);
        T!(c, d, a, b, input[14], S3, 2878612391);
        T!(b, c, d, a, input[ 5], S4, 4237533241);
        T!(a, b, c, d, input[12], S1, 1700485571);
        T!(d, a, b, c, input[ 3], S2, 2399980690);
        T!(c, d, a, b, input[10], S3, 4293915773);
        T!(b, c, d, a, input[ 1], S4, 2240044497);
        T!(a, b, c, d, input[ 8], S1, 1873313359);
        T!(d, a, b, c, input[15], S2, 4264355552);
        T!(c, d, a, b, input[ 6], S3, 2734768916);
        T!(b, c, d, a, input[13], S4, 1309151649);
        T!(a, b, c, d, input[ 4], S1, 4149444226);
        T!(d, a, b, c, input[11], S2, 3174756917);
        T!(c, d, a, b, input[ 2], S3,  718787259);
        T!(b, c, d, a, input[ 9], S4, 3951481745);
    }
    state[0] = add!(state[0], a);
    state[1] = add!(state[1], b);
    state[2] = add!(state[2], c);
    state[3] = add!(state[3], d);
}

impl Context {
    fn new() -> Self {
        Context {
            input: [0u8; 64],
            i: [0, 0],
            buf: [0x67452301, 0xefcdab89, 0x98badcfe, 0x10325476],
        }
    }

    fn update(&mut self, buf: &[u8]) {
        let mut input = [0u32; 16];
        let mut mdi = ((self.i[0] >> 3) & 0x3f) as usize;

        // Update # of bits
        let length = buf.len() as u32;

        if self.i[0].wrapping_add(length) << 3 < self.i[0] {
            self.i[1] =  self.i[1].wrapping_add(1);
        }

        self.i[0] = self.i[0].wrapping_add(length << 3);
        self.i[1] = self.i[1].wrapping_add(length >> 29);

        for &byte in buf {
            self.input[mdi] = byte;
            mdi += 1;

            if mdi == 0x40 {
                let mut j = 0;
                for i in 0..16 {
                    input[i] = ((self.input[j + 3] as u32) << 24) |
                        ((self.input[j + 2] as u32) << 16) |
                        ((self.input[j + 1] as u32) <<  8) |
                        ((self.input[j] as u32)      );
                    j += 4;
                }

                transform(&mut self.buf, &input);
                mdi = 0;
            }
        }
    }
}

/// Sign the DXIL blob with Mach-Siegbert-Vogt DXCSA
pub fn sign(blob: &[u8], out: &mut [u32; 4]) {
    const SECRET_HASH_OFFSET: usize = 20;
    let mut context = Context::new();

    // Note: first 4 bytes of bin are "DXBC" (IL) header/file-magic, then 16-byte signing hash,
    // then remainder of the file contents.
    let blob = &blob[SECRET_HASH_OFFSET..];

    let len = blob.len() as u32;
    let num_bits = len * 8;
    let full_chunks_size = len & 0xffffffc0;

    context.update(&blob[..full_chunks_size as usize]);

    let last_chunk_data = &blob[full_chunks_size as usize..];
    let padding_size = 64 - last_chunk_data.len();

    if last_chunk_data.len() >= 56 {
        context.update(last_chunk_data);

        // Pad to 56 mod 64
        context.update(&PADDING[0..padding_size]);

        let mut input = [0u32; 16];
        input[0] = num_bits;
        input[15] = (num_bits >> 2) | 1;
        transform(&mut context.buf, &input);
    } else {
        context.update(&num_bits.to_le_bytes());
        if last_chunk_data.len() != 0 {
            context.update(last_chunk_data);
        }

        // Adjust for the space used for num_bits
        let padding_size = padding_size - ::core::mem::size_of::<u32>();

        // Pad to 56 mod 64
        context.input[last_chunk_data.len()+::core::mem::size_of::<u32>()..].copy_from_slice(&PADDING[0..padding_size]);
        context.input[15..(15+::core::mem::size_of::<u32>())].copy_from_slice(&((num_bits >> 2) | 1).to_le_bytes());

        let mut input = [0u32; 16];
        input.copy_from_slice(bytemuck::cast_slice(&context.input));
        transform(&mut context.buf, &input);
    }

    out.copy_from_slice(&context.buf);
}

/// Sign the DXIL blob in place with Mach-Siegbert-Vogt DXCSA
pub fn sign_in_place(blob: &mut [u8]) {
    let mut signature = [0u32; 4];
    sign(&blob, &mut signature);
    blob[4..20].copy_from_slice(bytemuck::cast_slice(&signature));
}

#[cfg(test)]
mod test {
    use crate::sign;

    const REAL_SIGNED_BLOB: &[u8] = include_bytes!("../mipmap.dxil.blob");
    #[test]
    pub fn test() {
        let blob = REAL_SIGNED_BLOB.to_vec();
        let sig = &blob[4..20];
        let mut out_sig = [0u32; 4];

        sign(&blob, &mut out_sig);

        assert_eq!(sig, bytemuck::cast_slice(&out_sig));
    }

}