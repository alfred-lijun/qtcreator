Project {
    StaticLibrary {
        condition: {
            var result = qbs.targetPlatform === qbs.hostPlatform;
            if (!result)
                console.info("targetPlatform differs from hostPlatform");
            return result && hasProtobuf;
        }
        name: "proto_lib"

        Depends { name: "cpp" }
        cpp.cxxLanguageVersion: "c++11"
        cpp.minimumMacosVersion: "10.8"

        protobuf.cpp.importPaths: product.sourceDirectory

        Depends { name: "protobuf.cpp"; required: false }
        property bool hasProtobuf: {
            console.info("has protobuf: " + protobuf.cpp.present);
            return protobuf.cpp.present;
        }

        files: [
            "import.proto",
            "subdir/myenum.proto",
        ]

        Export {
            Depends { name: "cpp" }
            Depends { name: "protobuf.cpp"; required: false }
            cpp.cxxLanguageVersion: "c++11"
            cpp.minimumMacosVersion: "10.8"
            cpp.includePaths: exportingProduct.protobuf.cpp.outputDir
        }
    }

    CppApplication {
        condition: proto_lib.present
        name: "consumes_proto_lib"
        consoleApplication: true

        files: [
            "import-main.cpp",
        ]

        Depends {
            name: "proto_lib"
            required: false
        }
    }
}
