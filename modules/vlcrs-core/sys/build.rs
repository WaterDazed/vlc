use std::env;
use std::path::PathBuf;

fn main() {
    // // Tell cargo to tell rustc to link the system vlccore
    // // shared library.
    // //
    // // Note that this is only useful when doing out-of-tree or
    // // out of configure things and will not be used in tree.
    // println!("cargo:rustc-link-lib=vlccore");

    // Tell cargo to invalidate the built crate whenever the wrapper changes
    println!("cargo:rerun-if-changed=wrapper.h");

    // Create the bindings builder.
    let bindings = bindgen::Builder::default()
        // Where to find the header files are specified
        .clang_arg("-I../../../include")
        // Ignore some double defined macros
        .blocklist_item("FP_.*")
        // Make some types not copyable (probably internal-Pinning)
        .no_copy("input_item_t")
        // We don't control the input so we play safe and make them
        // non-exhaustive by default.
        .default_enum_style(bindgen::EnumVariation::Rust {
            non_exhaustive: true,
        })
        // We only allow these functions to be imported, the namespaces
        // have been found with:
        //  $ cat libvlccore.sym | cut -d'_' -f1 | sort | uniq
        .allowlist_function("vlc_.*")
        .allowlist_function("access_.*")
        .allowlist_function("aout_.*")
        .allowlist_function("config_.*")
        .allowlist_function("date_.*")
        .allowlist_function("decoder_.*")
        .allowlist_function("demux_.*")
        .allowlist_function("es_.*")
        .allowlist_function("filter_.*")
        .allowlist_function("image_.*")
        .allowlist_function("input_.*")
        .allowlist_function("module_.*")
        .allowlist_function("net_.*")
        .allowlist_function("picture_.*")
        .allowlist_function("plane_.*")
        .allowlist_function("sout_.*")
        .allowlist_function("spu_.*")
        .allowlist_function("subpicture_.*")
        .allowlist_function("text_.*")
        .allowlist_function("var_.*")
        .allowlist_function("video_.*")
        .allowlist_function("vlm_.*")
        .allowlist_function("vout_.*")
        .allowlist_function("xml_.*")
        .allowlist_function("VLC_.*")
        // This seems to be required otherwise most (all ?) of the type
        // under the vlc_ namespace are not present. It's probably due
        // to the untyped API in vlc.
        .allowlist_type("vlc_.*")
        .allowlist_type("VLC_.*")
        .allowlist_type("es_.*")
        .allowlist_type("demux_.*")
        .allowlist_type("module_.*")
        .allowlist_type("stream_.*")
        .allowlist_type("input_.*")
        // This allows variables (const, static and simple #define).
        // Manually enableing them here is required because those are
        // almost never referenced directly as a function parameter or
        // in a struct definition.
        .allowlist_var("VLC_.*")
        .allowlist_var("CONFIG_.*")
        .allowlist_var("CLOCK_.*")
        // The input header we would like to generate
        // bindings for.
        .header("wrapper.h")
        // Tell cargo to invalidate the built crate whenever any of the
        // included header files changed.
        .parse_callbacks(Box::new(bindgen::CargoCallbacks))
        // Finish the builder and generate the bindings.
        .generate()
        // Unwrap the result and panic on failure.
        .expect("Unable to generate bindings");

    // Write the bindings to the $OUT_DIR/bindings.rs file.
    let out_path = PathBuf::from(env::var("OUT_DIR").unwrap());
    bindings
        .write_to_file(out_path.join("bindings.rs"))
        .expect("Couldn't write bindings!");
}
