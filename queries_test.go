package tree_sitter_containerfile_test

import (
	"testing"

	containerfile "github.com/wharflab/tree-sitter-containerfile"
)

func TestHighlightsQueryCompiles(t *testing.T) {
	if containerfile.HighlightsQuery == "" {
		t.Fatal("HighlightsQuery is empty")
	}
	query, err := containerfile.GetHighlightsQuery()
	if err != nil {
		t.Fatalf("highlights query failed to compile: %v", err)
	}
	defer query.Close()

	names := query.CaptureNames()
	if len(names) == 0 {
		t.Fatal("highlights query has no captures")
	}

	// Verify keyword capture exists
	found := false
	for _, name := range names {
		if name == "keyword" {
			found = true
			break
		}
	}
	if !found {
		t.Errorf("expected keyword capture, got: %v", names)
	}
}
