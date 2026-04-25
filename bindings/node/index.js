import { readFileSync } from "node:fs";
import { fileURLToPath } from "node:url";

const rootURL = new URL("../..", import.meta.url);
const root = fileURLToPath(rootURL);

const binding = typeof process.versions.bun === "string"
  // Support `bun build --compile` by being statically analyzable enough to find the .node file at build-time
  ? (await import(new URL(`prebuilds/${process.platform}-${process.arch}/tree-sitter-containerfile.node`, rootURL).href)).default
  : (await import("node-gyp-build")).default(root);

try {
  const nodeTypes = await import(new URL("src/node-types.json", rootURL).href, { with: { type: "json" } });
  binding.nodeTypeInfo = nodeTypes.default;
} catch { }

const queries = [
  ["HIGHLIGHTS_QUERY", `${root}/queries/highlights.scm`],
  ["INJECTIONS_QUERY", `${root}/queries/injections.scm`],
  ["LOCALS_QUERY", `${root}/queries/locals.scm`],
  ["TAGS_QUERY", `${root}/queries/tags.scm`],
];

for (const [prop, path] of queries) {
  Object.defineProperty(binding, prop, {
    configurable: true,
    enumerable: true,
    get() {
      delete binding[prop];
      try {
        binding[prop] = readFileSync(path, "utf8");
      } catch { }
      return binding[prop];
    }
  });
}

export default binding;
