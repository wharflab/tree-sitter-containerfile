import XCTest
import SwiftTreeSitter
import TreeSitterContainerfile

final class TreeSitterContainerfileTests: XCTestCase {
    func testCanLoadGrammar() throws {
        let parser = Parser()
        let language = Language(language: tree_sitter_containerfile())
        XCTAssertNoThrow(try parser.setLanguage(language),
                         "Error loading Containerfile grammar")
    }
}
