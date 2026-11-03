// Runs entries of a langc Wasm module and prints each result as `langc eval`
// does: a number, or `trap` when the module traps.
// Usage: node test/run-wasm.mjs [--wasmtime] FILE NAME [ARGS...]
//        node test/run-wasm.mjs [--wasmtime] FILE -   (one `NAME [ARGS...]` line on stdin per call)
//        node test/run-wasm.mjs [--wasmtime] --decide SCRIPT FILE
//        node test/run-wasm.mjs --lang SCRIPT
// --wasmtime makes each call with `wasmtime run --invoke` (a new instance for
// each call; the words go as signed i64) in place of node. A module that
// imports module permit gets the host imports below (S8): from node, or from
// a stub module that wat2wasm makes, with `within` as a table of the
// permit.res paths.
// A decide script (S8 U3, Q-S8-12 (a)) has one call per line: `decide PATH
// ACTION PAYLOAD COST DIGEST` (DIGEST `-` is no digest, `PATH:N` is the
// digest hash (envelope (res PATH) (payloadOf N))) or `malformed`. --decide
// makes the calls on one instance of FILE and prints the codes one per line,
// then `steps N` and `left N` (words 0 and 1 of the exported memory). --lang
// prints the calls as defs to append to a copy of examples/gate.lang: the
// judge chain on start, cK : Decision for line K, then steps and left.
import { mkdtempSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { spawnSync } from "node:child_process";
import { tmpdir } from "node:os";
import { join } from "node:path";

const usage = () => {
  process.stderr.write(
    "usage: node test/run-wasm.mjs [--wasmtime] FILE NAME [ARGS...] | [--wasmtime] FILE - |" +
      " [--wasmtime] --decide SCRIPT FILE | --lang SCRIPT\n",
  );
  process.exit(2);
};

const mask = (1n << 64n) - 1n;
const u64 = (w) => BigInt.asUintN(64, BigInt(w));
const s64 = (w) => BigInt.asIntN(64, BigInt(w)).toString();

// permit.res: a uleb count, then each path as a uleb length and its bytes.
const uleb = (b, at) => {
  const go = (i, shift, v) =>
    (b[i] & 0x80) === 0 ? [v | ((b[i] & 0x7f) << shift), i + 1] : go(i + 1, shift + 7, v | ((b[i] & 0x7f) << shift));
  return go(at, 0, 0);
};
const resPaths = (mod) =>
  WebAssembly.Module.customSections(mod, "permit.res").slice(0, 1).flatMap((section) => {
    const b = new Uint8Array(section);
    const [count, start] = uleb(b, 0);
    return Array.from({ length: count }).reduce(
      ([paths, at]) => {
        const [len, from] = uleb(b, at);
        return [[...paths, new TextDecoder().decode(b.subarray(from, from + len))], from + len];
      },
      [[], start],
    )[0];
  });
const hasHost = (mod) => WebAssembly.Module.imports(mod).some((i) => i.module === "permit");

// The host imports (src/ir.h IR_IMPORTS): envelope t p, hash e, and within r
// g, 1 when path g is path r or an ancestor of r.
const inside = (r, g) => r === g || r.startsWith(g + "/");
const envelope = (t, p) => ((u64(t) << 32n) | u64(p)) & mask;
const hash = (e) => (u64(e) * 1000003n + 7n) & mask;
const host = (paths) => ({
  permit: { envelope, hash, within: (r, g) => (inside(paths[Number(r)], paths[Number(g)]) ? 1n : 0n) },
});

const withDir = (f) => {
  const dir = mkdtempSync(join(tmpdir(), "run-wasm-"));
  try {
    return f(dir);
  } finally {
    rmSync(dir, { recursive: true, force: true });
  }
};
const wat2wasm = (dir, name, wat) => {
  const src = join(dir, `${name}.wat`);
  const out = join(dir, `${name}.wasm`);
  writeFileSync(src, wat);
  const r = spawnSync("wat2wasm", [src, "-o", out], { encoding: "utf8" });
  if (r.status !== 0) throw new Error(`wat2wasm ${name}: ${r.stderr}`);
  return out;
};
// The stub module permit for wasmtime: the same host functions; within reads
// byte r * n + g of the table of the n permit.res paths.
const stub = (dir, paths) =>
  wat2wasm(
    dir,
    "permit",
    `(module
  (memory 1)
  (data (i32.const 0) "${paths.flatMap((r) => paths.map((g) => (inside(r, g) ? "\\01" : "\\00"))).join("")}")
  (func (export "envelope") (param i64 i64) (result i64)
    (i64.or (i64.shl (local.get 0) (i64.const 32)) (local.get 1)))
  (func (export "hash") (param i64) (result i64)
    (i64.add (i64.mul (local.get 0) (i64.const 1000003)) (i64.const 7)))
  (func (export "within") (param i64 i64) (result i64)
    (i64.load8_u (i32.wrap_i64 (i64.add (i64.mul (local.get 0) (i64.const ${paths.length})) (local.get 1))))))
`,
  );
const wasmtime = (args) => {
  const r = spawnSync("wasmtime", ["run", ...args], { encoding: "utf8" });
  if (r.status !== 0 && !/trap/.test(r.stderr)) throw new Error(`wasmtime exit ${r.status}: ${r.stderr.split("\n")[0]}`);
  return r.status === 0 ? r.stdout.split("\n").filter((line) => line !== "").map((line) => u64(line).toString()) : ["trap"];
};

const nodeCall = (instance, [entry, ...words]) => {
  const fn = instance.exports[entry];
  if (typeof fn !== "function") throw new Error(`no exported entry '${entry}'`);
  try {
    return BigInt.asUintN(64, fn(...words.map(BigInt))).toString();
  } catch (e) {
    if (e instanceof WebAssembly.RuntimeError) return "trap";
    throw e;
  }
};
const runCalls = (file, calls, viaWasmtime) => {
  const mod = new WebAssembly.Module(readFileSync(file));
  const preload = (dir) => (hasHost(mod) ? ["--preload", `permit=${stub(dir, resPaths(mod))}`] : []);
  if (!viaWasmtime) {
    const instance = new WebAssembly.Instance(mod, host(resPaths(mod)));
    return calls.map((c) => nodeCall(instance, c));
  }
  return withDir((dir) => {
    const pre = preload(dir);
    return calls.map(([entry, ...words]) => wasmtime([...pre, "--invoke", entry, file, ...words.map(s64)]).join(" "));
  });
};

// A decide script line: `decide PATH ACTION PAYLOAD COST DIGEST` or `malformed`.
const ACTIONS = ["inspect", "propose", "execute"]; // family Action, domain/domain.lang
const script = (file) =>
  readFileSync(file, "utf8")
    .split("\n")
    .filter((line) => line !== "")
    .map((line) => line.split(" "))
    .map((l) => {
      const ok =
        (l[0] === "malformed" && l.length === 1) ||
        (l[0] === "decide" && l.length === 6 && ACTIONS.includes(l[2]) && /^\d+$/.test(l[3]) && /^\d+$/.test(l[4]) &&
          /^(-|[^:]+:\d+)$/.test(l[5]));
      if (!ok) throw new Error(`${file}: bad line '${l.join(" ")}'`);
      return l;
    });
const handle = (paths, p) => {
  const i = paths.indexOf(p);
  if (i < 0) throw new Error(`no permit.res path '${p}'`);
  return BigInt(i);
};
const digestWords = (paths, d) => {
  const at = d.indexOf(":");
  return d === "-" ? [0n, 0n] : [1n, hash(envelope(handle(paths, d.slice(0, at)), BigInt(d.slice(at + 1))))];
};
// The 6 params of decide: target, action, payload, cost, has_digest, digest.
const decideWords = (paths, [, path, action, payload, cost, digest]) => [
  handle(paths, path),
  BigInt(ACTIONS.indexOf(action)),
  BigInt(payload),
  BigInt(cost),
  ...digestWords(paths, digest),
];
// The driver module for wasmtime: run makes the calls on the gate instance
// and returns the codes, then the words 0 and 1 of gate.memory.
const driver = (paths, lines) => `(module
  (import "gate" "decide" (func $decide (param i64 i64 i64 i64 i64 i64) (result i64)))
  (import "gate" "malformed" (func $malformed (result i64)))
  (import "gate" "memory" (memory 1))
  (func (export "run") (result${" i64".repeat(lines.length + 2)})
${lines
  .map((l) =>
    l[0] === "malformed"
      ? "    (call $malformed)"
      : `    (call $decide${decideWords(paths, l).map((w) => ` (i64.const ${s64(w)})`).join("")})`,
  )
  .join("\n")}
    (i64.load (i32.const 0))
    (i64.load (i32.const 8))))
`;
const runDecide = (file, lines, viaWasmtime) => {
  const mod = new WebAssembly.Module(readFileSync(file));
  const paths = resPaths(mod);
  const words = viaWasmtime
    ? withDir((dir) =>
        wasmtime(["--preload", `permit=${stub(dir, paths)}`, "--preload", `gate=${file}`, "--invoke", "run", wat2wasm(dir, "drive", driver(paths, lines))]),
      )
    : (() => {
        const x = new WebAssembly.Instance(mod, host(paths)).exports;
        const codes = lines.map((l) => (l[0] === "malformed" ? x.malformed() : x.decide(...decideWords(paths, l))));
        const view = new DataView(x.memory.buffer);
        return [...codes, view.getBigUint64(0, true), view.getBigUint64(8, true)].map((w) => u64(w).toString());
      })();
  return [...words.slice(0, lines.length), `steps ${words[lines.length]}`, `left ${words[lines.length + 1]}`];
};

const digestLang = (d) => {
  const at = d.indexOf(":");
  return d === "-" ? "none" : `(some (hash (envelope (res "${d.slice(0, at)}") (payloadOf ${d.slice(at + 1)}))))`;
};
const lang = (file, lines) => {
  const auth = (k) => (k === 0 ? "start" : `(second j${k})`);
  return [
    `-- The judge chain of ${file} (node test/run-wasm.mjs --lang).`,
    ...lines.flatMap((l, i) => [
      `def j${i + 1} : Prod Decision Authority := ${
        l[0] === "malformed"
          ? `judgeMalformed ${auth(i)}`
          : `judge ${auth(i)} (ticket (mutation (res "${l[1]}") ${l[2]} (payloadOf ${l[3]})) ${l[4]} ${digestLang(l[5])})`
      }`,
      `def c${i + 1} : Decision := first j${i + 1}`,
    ]),
    `def steps : StepBound := auSteps ${auth(lines.length)}`,
    `def left : Nat := auLeft ${auth(lines.length)}`,
  ];
};

const argv = process.argv.slice(2);
const viaWasmtime = argv[0] === "--wasmtime";
const [first, second, ...rest] = viaWasmtime ? argv.slice(1) : argv;
const lines = (() => {
  if (first === undefined || second === undefined) return usage();
  if (first === "--lang") return viaWasmtime || rest.length !== 0 ? usage() : lang(second, script(second));
  if (first === "--decide") return rest.length !== 1 ? usage() : runDecide(rest[0], script(second), viaWasmtime);
  const calls = second === "-"
    ? readFileSync(0, "utf8").split("\n").filter((line) => line !== "").map((line) => line.split(" "))
    : [[second, ...rest]];
  return runCalls(first, calls, viaWasmtime);
})();
process.stdout.write(lines.map((line) => line + "\n").join(""));
