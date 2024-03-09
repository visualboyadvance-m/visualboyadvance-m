use core::ffi::c_char;
use core::ffi::c_int;
use core::ffi::CStr;

use crate::base::StrLike;
use crate::prelude::*;

#[repr(C)]
pub enum GbaCheatsType {
    Generic,
    GameSharkV2,
    CodeBreaker,
    ActionReplay,
}

impl GbaCheatsType {
    #[no_mangle]
    pub extern "C" fn some_type() -> GbaCheatsType {
        GbaCheatsType::Generic
    }
}

#[repr(C)]
pub struct CheatsData {
    code: c_int,
    size: c_int,
    status: c_int,
    enabled: bool,
    rawaddress: u32,
    address: u32,
    value: u32,
    old_value: u32,
    codestring: [c_char; 20],
    desc: [c_char; 32],
}

#[repr(C)]
pub enum CheatsDecodeError {
    UnsupportedType,
    InvalidLength,
    InvalidCharacter,
    AddressNotHex,
    ValueNotHex,
}

impl CheatsData {
    fn decode_generic(code_string: &CStr, description: &CStr) -> Result<CheatsData, CheatsDecodeError> {
        let code_length = code_string.to_bytes().len();
        if code_length != 11 && code_length != 13 && code_length != 17 {
            return Err(CheatsDecodeError::InvalidLength);
        }

        if code_string.to_bytes()[8] != b':' {
            return Err(CheatsDecodeError::InvalidCharacter);
        }

        let address = 
        match code_string.to_bytes()[..8].to_hex() {
            Some(address) => address,
            None => {
                return Err(CheatsDecodeError::AddressNotHex);
            }
        };

        let value =
        match code_string.to_bytes()[9..].to_hex() {
            Some(value) => value,
            None => {
                return Err(CheatsDecodeError::ValueNotHex);
            }
        };

        let code = if code_length == 1 {
            0
        } else if code_length == 13 {
        114
        } else {
            115
        };

        Ok(CheatsData {
            code,
            size: code,
            status: 0,
            enabled: true,
            rawaddress: address,
            address,
            value,
            old_value: 0,
            codestring: code_string.clone_buffer(),
            desc: description.clone_buffer(),
        })
    }

    #[no_mangle]
    pub extern "C" fn decode_code(
        cheat_type: GbaCheatsType,
        code_string: *const c_char,
        description: *const c_char,
    ) -> Result<CheatsData, CheatsDecodeError> {
        let code = unsafe { CStr::from_ptr(code_string) };
        let description = unsafe { CStr::from_ptr(description) };
        match cheat_type {
            GbaCheatsType::Generic => CheatsData::decode_generic(code, description),
            _ => Err(CheatsDecodeError::UnsupportedType),
        }
    }
}
