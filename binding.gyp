{
  "targets": [{
    "target_name": "iag_native",
    "sources": ["src/binding.cc", "deps/ascii-gen-c/src/converter.c"],
    "include_dirs": [
      "deps/ascii-gen-c/include",
      "<!(node -p \"require('node-addon-api').include_dir\")"
    ],
    "cflags": ["-O2", "-std=c11"],
    "cflags_cc": ["-O2"],
    "defines": ["NAPI_DISABLE_CPP_EXCEPTIONS"]
  }]
}
