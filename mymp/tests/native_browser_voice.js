#!/usr/bin/env node
/* Compatibility test: pages produced by the native GTA client voice core
   (C++/libopus, /tmp/native_pages.bin) must decode in the BROWSER decoder
   (opus-recorder wasm). If this passes, GTA V players and browser players
   hear each other with zero special-casing. */
const { Worker } = require('worker_threads');
const path = require('path');
const fs = require('fs');

const DIR = '/home/user/mymp/mymp/web/vendor/opus';

const shim = `
const { parentPort, workerData } = require('worker_threads');
const path = require('path');
globalThis.postMessage = (m, t) => parentPort.postMessage(m);
globalThis.locateFile = (p) => path.join(workerData.dir, p);
globalThis.fetch = undefined;
globalThis.close = () => process.exit(0);
let handler = null;
Object.defineProperty(globalThis, 'onmessage', {
  set(fn) { handler = fn; }, get() { return handler; }, configurable: true
});
parentPort.on('message', m => { if (handler) handler({ data: m }); });
setInterval(() => {}, 1000);
`;
const code = fs.readFileSync(path.join(DIR, 'decoderWorker.min.js'), 'utf8');
const dec = new Worker(shim + code, { eval: true, workerData: { dir: DIR } });
dec.on('error', e => { console.error('DEC WORKER ERROR:', e.message); process.exit(1); });

const blob = fs.readFileSync('/tmp/native_pages.bin');
console.log('native pages blob:', blob.length, 'bytes');

const out = [];
let total = 0, peak = 0;
dec.on('message', m => {
  if (Array.isArray(m)) for (const b of m) {
    const f = new Float32Array(b);
    total += f.length;
    for (let i = 0; i < f.length; i++) peak = Math.max(peak, Math.abs(f[i]));
  }
});
dec.postMessage({ command: 'init', decoderSampleRate: 16000,
                  outputBufferSampleRate: 16000, resampleQuality: 3 });
setTimeout(() => {
  dec.postMessage({ command: 'decode', pages: new Uint8Array(blob) });
}, 300);
setTimeout(() => {
  console.log(`browser decoder got ${total} samples, peak ${peak.toFixed(3)}`);
  const ok = total >= 12000 && peak > 0.2 && peak <= 1.0;
  console.log(ok ? 'NATIVE->BROWSER COMPAT PASSED ✅' : 'NATIVE->BROWSER COMPAT FAILED ❌');
  process.exit(ok ? 0 : 1);
}, 3000);
