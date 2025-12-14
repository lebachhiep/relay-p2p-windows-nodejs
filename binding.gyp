{
  "targets": [
    {
      "target_name": "relay_leaf",
      "sources": [ "src/relay_leaf.cc" ],

      "include_dirs": [
        "<(node_root_dir)",
        "<(node_root_dir)/src",
        "<(node_root_dir)/deps/v8/include",
        "<!(node -p \"require('node-addon-api').include_dir\")"
      ],

      "dependencies": [
        "<!(node -p \"require('node-addon-api').gyp\")"
      ],

      "defines": [
        "NAPI_DISABLE_CPP_EXCEPTIONS"
      ],

      "cflags_cc!": [ "-fno-exceptions" ],

      "msvs_settings": {
        "VCCLCompilerTool": {
          "ExceptionHandling": 1
        }
      }
    }
  ]
}
