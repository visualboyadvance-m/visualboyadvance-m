extern crate cbindgen;

use std::env;

fn main() {
    let crate_dir = env::var("CARGO_MANIFEST_DIR").unwrap();

    cbindgen::Builder::new()
      .with_crate(crate_dir)
      .with_language(cbindgen::Language::C)
      .with_cpp_compat(true)
      .with_include_guard("VBAM_CORE_RUST_H_")
      .with_namespace("core")
      .generate()
      .expect("Unable to generate bindings")
      .write_to_file("bindings.hpp");
}
