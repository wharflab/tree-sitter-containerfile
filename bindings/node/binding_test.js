import assert from "node:assert";
import { test } from "node:test";

test("can load grammar", () => {
  assert.doesNotReject(async () => {
    const { default: binding } = await import("./index.js");
    assert.ok(binding.language);
    assert.ok(Array.isArray(binding.nodeTypeInfo));
  });
});
