{
  "targets": [
    {
      "target_name": "bqlog",
      "sources": [
        "src/bqlog_node_api.cpp"
      ],
      "include_dirs": [
        "<!@(node -p \"require('node-addon-api').include\")",
        "../../include",
        "include"
      ],
      "dependencies": [
        "<!(node -p \"require('node-addon-api').gyp\")"
      ],
      "cflags!": [ "-fno-exceptions" ],
      "cflags_cc!": [ "-fno-exceptions" ],
      "defines": [
        "NAPI_DISABLE_CPP_EXCEPTIONS",
        "BQ_JAVA=0"
      ],
      "conditions": [
        ["OS=='win'", {
          "defines": ["_HAS_EXCEPTIONS=1"],
          "msvs_settings": {
            "VCCLCompilerTool": { "ExceptionHandling": 1 }
          }
        }],
        ["OS=='mac'", {
          "xcode_settings": {
            "GCC_ENABLE_CPP_EXCEPTIONS": "YES",
            "CLANG_CXX_LIBRARY": "libc++",
            "MACOSX_DEPLOYMENT_TARGET": "10.7"
          }
        }],
        ["OS=='linux'", {
          "cflags_cc": ["-std=c++17"]
        }]
      ],
      "libraries": [
        # Link to BqLog static library - adjust path as needed
        # "-L../../dist/dynamic_lib/<(OS)_<(target_arch)",
        # "-lbqlog"
      ],
      "library_dirs": [
        "../../dist/dynamic_lib"
      ]
    }
  ]
}