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

func TestInjectionsQueryCompiles(t *testing.T) {
	if containerfile.InjectionsQuery == "" {
		t.Fatal("InjectionsQuery is empty")
	}
	query, err := containerfile.GetInjectionsQuery()
	if err != nil {
		t.Fatalf("injections query failed to compile: %v", err)
	}
	defer query.Close()

	names := query.CaptureNames()
	if len(names) == 0 {
		t.Fatal("injections query has no captures")
	}

	found := false
	for _, name := range names {
		if name == "injection.content" {
			found = true
			break
		}
	}
	if !found {
		t.Errorf("expected injection.content capture, got: %v", names)
	}
}
