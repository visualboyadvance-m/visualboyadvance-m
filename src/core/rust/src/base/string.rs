use core::ffi::c_char;
use core::ffi::CStr;

pub(crate) trait CharLike {
    fn to_hex(&self) -> Option<u8>;
    fn is_hex(&self) -> bool {
        self.to_hex().is_some()
    }
}

impl CharLike for u8 {
    fn to_hex(&self) -> Option<u8> {
        if *self >= b'0' && *self <= b'9' {
            Some(*self - b'0')
        } else if *self >= b'A' && *self <= b'F' {
            Some(*self - b'A' + 10)
        } else {
            None
        }
    }
}

pub(crate) trait StrLike {
    fn to_hex(&self) -> Option<u32>;
    fn clone_buffer<const LEN: usize>(&self) -> [c_char; LEN];
}

impl StrLike for [u8] {
    fn to_hex(&self) -> Option<u32> {
        let mut result: u32 = 0;
        for byte in self.iter() {
            if let Some(value) = byte.to_hex() {
                result = result.wrapping_mul(16).wrapping_add(value as u32);
            } else {
                return None;
            }
        }
        Some(result)
    }

    fn clone_buffer<const LEN: usize>(&self) -> [c_char; LEN] {
        let mut buffer = [0; LEN];
        let mut i: usize = 0;
        for byte in self.iter() {
            if i == LEN {
                break;
            }
            buffer[i] = *byte as c_char;
            i += 1;
        }
        if i == LEN {
            buffer[LEN - 1] = 0;
        } else {
            buffer[i] = 0;
        }
        buffer
    }
}

impl StrLike for CStr {
    fn to_hex(&self) -> Option<u32> {
        self.to_bytes().to_hex()
    }

    fn clone_buffer<const LEN: usize>(&self) -> [c_char; LEN] {
        self.to_bytes().clone_buffer()
    }
}

#[cfg(test)]
mod test {
    use super::*;

    #[test]
    fn test_char_to_hex() {
        assert_eq!(b'0'.to_hex(), Some(0));
        assert_eq!(b'9'.to_hex(), Some(9));
        assert_eq!(b'A'.to_hex(), Some(10));
        assert_eq!(b'F'.to_hex(), Some(15));
        assert_eq!(b'G'.to_hex(), None);
    }

    #[test]
    fn test_str_to_hex() {
        assert_eq!([].to_hex(), Some(0));
        assert_eq!([b'0'].to_hex(), Some(0));
        assert_eq!([b'1', b'0'].to_hex(), Some(16));
        assert_eq!([b'F', b'F'].to_hex(), Some(255));
        assert_eq!([b'G'].to_hex(), None);
        assert_eq!(b"ABCDEF".to_hex(), Some(11259375));
    }

    #[test]
    fn test_str_clone_buffer() {
        assert_eq!(b"test".clone_buffer(), [116, 101, 115, 116, 0]);
        assert_eq!(b"test".clone_buffer(), [116, 101, 0]);
        assert_eq!(b"test".clone_buffer(), [0]);
        assert_eq!(b"".clone_buffer(), [0]);
    }
}
