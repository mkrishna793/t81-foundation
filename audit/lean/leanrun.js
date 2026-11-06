#!/usr/bin/env node
/**
 * Headless Lean 4 runner on the lean4-wasm (Emscripten) build.
 * usage: node leanrun.js <file.lean> [extra lean flags...]
 * Needs Node >= 22 (V8 wasm export limit raised).
 */
const fs = require('fs');
const path = require('path');
const HOME = process.env.LEAN4_WASM_HOME || '/home/user/leanwasm';
const WASM = path.join(HOME, 'node_modules', 'lean4-wasm', 'lean.wasm');
const file = process.argv[2];
if (!file) { console.error('usage: node leanrun.js <file.lean>'); process.exit(2); }
const extraArgs = process.argv.slice(3);
process.env.LEAN_PATH = path.join(HOME, 'node_modules', 'lean4-wasm', 'lib');
global.Module = {
  wasmBinaryFile: WASM,
  locateFile: (p) => path.join(path.dirname(WASM), p),
  noInitialRun: true,
  print: (t) => console.log(t),
  printErr: (t) => console.error(t),
  onRuntimeInitialized: function () {
    try { process.exitCode = Module.callMain([file, ...extraArgs]) || 0; }
    catch (e) {
      if (e && typeof e.status === 'number') process.exitCode = e.status;
      else { console.error('ERR', e && e.message ? e.message : e); process.exitCode = 1; }
    }
  },
  onAbort: (w) => { console.error('ABORT', w); process.exit(1); },
};
process.on('unhandledRejection', (e) => { console.error('REJECT', e); process.exit(1); });
require(path.join(HOME, 'node_modules', 'lean4-wasm', 'lean.js'));
