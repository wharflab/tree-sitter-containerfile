import assert from "node:assert";
import { test } from "node:test";

test("can load grammar", async () => {
  await assert.doesNotReject(async () => {
    const { default: binding } = await import("./index.js");
    assert.ok(binding.language);
    assert.ok(Array.isArray(binding.nodeTypeInfo));
    assert.ok(binding.HIGHLIGHTS_QUERY);
    assert.ok(binding.INJECTIONS_QUERY);
  });
});
