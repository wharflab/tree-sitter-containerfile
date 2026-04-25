// swift-tools-version:5.9

import PackageDescription

let package = Package(
    name: "TreeSitterContainerfile",
    products: [
        .library(name: "TreeSitterContainerfile", targets: ["TreeSitterContainerfile"]),
    ],
    dependencies: [
        .package(name: "SwiftTreeSitter", url: "https://github.com/tree-sitter/swift-tree-sitter", from: "0.25.0"),
    ],
    targets: [
        .target(
            name: "TreeSitterContainerfile",
            dependencies: [],
            path: ".",
            exclude: [
                ".editorconfig",
                ".gitattributes",
                ".github",
                "binding.gyp",
                "bindings/c",
                "bindings/go",
                "bindings/node",
                "bindings/python",
                "bindings/rust",
                "Cargo.lock",
                "Cargo.toml",
                "CMakeLists.txt",
                "eslint.config.mjs",
                "examples",
                "go-compat",
                "go.mod",
                "go.sum",
                "grammar.js",
                "LICENSE",
                "LICENSE-MIT-CamdenCheek",
                "Makefile",
                "node_types.go",
                "package-lock.json",
                "package.json",
                "pyproject.toml",
                "queries.go",
                "queries_test.go",
                "README.md",
                "renovate.json",
                "setup.py",
                "src/grammar.json",
                "src/node-types.json",
                "test",
                "tree-sitter.json",
            ],
            sources: [
                "src/parser.c",
                "src/scanner.c",
            ],
            resources: [
                .copy("queries"),
            ],
            publicHeadersPath: "bindings/swift",
            cSettings: [.headerSearchPath("src")],
        ),
        .testTarget(
            name: "TreeSitterContainerfileTests",
            dependencies: [
                "SwiftTreeSitter",
                "TreeSitterContainerfile",
            ],
            path: "bindings/swift/TreeSitterContainerfileTests",
        ),
    ],
    cLanguageStandard: .c11
)
