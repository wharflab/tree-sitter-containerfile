package tree_sitter_containerfile

import (
	_ "embed"

	binding "github.com/wharflab/tree-sitter-containerfile/bindings/go"
	sitter "github.com/tree-sitter/go-tree-sitter"
)

//go:embed queries/highlights.scm
var HighlightsQuery string

//go:embed queries/injections.scm
var InjectionsQuery string

// GetLanguage returns the tree-sitter Language for Containerfile.
func GetLanguage() *sitter.Language {
	return sitter.NewLanguage(binding.Language())
}

// GetHighlightsQuery compiles and returns the bundled highlights query.
func GetHighlightsQuery() (*sitter.Query, *sitter.QueryError) {
	return sitter.NewQuery(GetLanguage(), HighlightsQuery)
}

// GetInjectionsQuery compiles and returns the bundled injections query.
func GetInjectionsQuery() (*sitter.Query, *sitter.QueryError) {
	return sitter.NewQuery(GetLanguage(), InjectionsQuery)
}
