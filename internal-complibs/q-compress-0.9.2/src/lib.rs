#[no_mangle]
pub extern "C" fn q_compress_ffi(
  input: *const u8,
  input_len: i32,
  output: *mut u8,
  output_len: i32,
  meta: u8,
  chunk: *const std::ffi::c_void,
) -> i32 {
  0
}

#[no_mangle]
pub extern "C" fn q_decompress_ffi(
  input: *const u8,
  input_len: i32,
  output: *mut u8,
  output_len: i32,
  meta: u8,
  chunk: *const std::ffi::c_void,
) -> i32 {
  0
}
