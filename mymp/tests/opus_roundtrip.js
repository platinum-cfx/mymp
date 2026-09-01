#!/usr/bin/env node
/* Roundtrip test: opus-recorder encoder worker -> decoder worker in Node.
   Proves the wasm workers work end-to-end (16k mono in -> 16k mono out). */
const { Worker, workerData } = require('worker_threads');
const path = require('path');
const fs = require('fs');

const DIR = '/home/user/mymp/mymp/web/vendor/opus';

function makeWorker(file, setup) {
  const code = fs.readFileSync(path.join(DIR, file), 'utf8');
  const shim = `
const { parentPort, workerData } = require('worker_threads');
const path = require('path');
globalThis.postMessage = (m, t) => parentPort.postMessage(m);
globalThis.locateFile = (p) => path.join(workerData.dir, p);
globalThis.fetch = undefined; // force node fs path (worker threads have fetch)
globalThis.close = () => process.exit(0);
// bridge worker_threads messages to the global onmessage the workers set up
let handler = null;
Object.defineProperty(globalThis, 'onmessage', {
  set(fn) { handler = fn; }, get() { return handler; }, configurable: true
});
parentPort.on('message', m => { if (handler) handler({ data: m }); });
setInterval(() => {}, 1000); // keep the worker's event loop alive (browsers do this implicitly)
`;
  const w = new Worker(shim + code, { eval: true, workerData: { dir: DIR } });
  return w;
}

const enc = makeWorker('encoderWorker.min.js');
const dec = makeWorker('decoderWorker.min.js');
enc.on('error', e => { console.error('ENC WORKER ERROR:', e.message); process.exit(1); });
dec.on('error', e => { console.error('DEC WORKER ERROR:', e.message); process.exit(1); });
enc.on('exit', c => console.log('enc worker exited', c));
dec.on('exit', c => console.log('dec worker exited', c));

const encReady = new Promise(r => enc.on('message', m => { if (m && m.message === 'ready') r(); }));
// decoder init sends no reply (by design) — give it a beat
const decReady = new Promise(r => setTimeout(r, 500));

(async () => {
  enc.on('message', m => console.log('[enc->]', m && (m.message || (Array.isArray(m) ? 'arr' + m.length : typeof m)), m && m.page ? m.page.length : ''));
  dec.on('message', m => console.log('[dec->]', Array.isArray(m) ? 'arr ' + m.length : m && m.message || m));
  enc.postMessage({ command: 'init',
    encoderApplication: 2048, encoderFrameSize: 20, encoderSampleRate: 16000,
    originalSampleRate: 48000, numberOfChannels: 1, maxFramesPerPage: 1,
    resampleQuality: 3 });
  dec.postMessage({ command: 'init', decoderSampleRate: 16000,
    outputBufferSampleRate: 16000, resampleQuality: 3 });
  await encReady; await decReady;
  console.log('both workers ready');

  // header pages (Ogg ID + comment) — needed by the decoder to init
  const hdrPages = [];
  const hdrCollector = m => { if (m && m.message === 'page' && m.page && hdrPages.length < 2) hdrPages.push(new Uint8Array(m.page)); };
  enc.on('message', hdrCollector);
  enc.postMessage({ command: 'getHeaderPages' });
  await new Promise(r => setTimeout(r, 300));
  enc.removeListener('message', hdrCollector);

  // encode: 40 frames of 20ms = 0.8s of 440Hz sine at 48k input
  const out = [];
  dec.on('message', m => {
    if (Array.isArray(m)) for (const b of m) out.push(new Float32Array(b));
  });
  const frames = [];
  for (let f = 0; f < 40; f++) {
    const chunk = new Float32Array(960); // 20ms @ 48k
    for (let i = 0; i < chunk.length; i++)
      chunk[i] = Math.sin(2 * Math.PI * 440 * (f * 960 + i) / 48000) * 0.5;
    frames.push(chunk);
  }
  // feed header pages to decoder first (BOS init), then encode+decode live.
  // NOTE: the decoder expects ONE Uint8Array of concatenated Ogg pages.
  const concat = (arrs) => {
    const total = arrs.reduce((s, a) => s + a.length, 0);
    const out = new Uint8Array(total);
    let o = 0;
    for (const a of arrs) { out.set(a, o); o += a.length; }
    return out;
  };
  dec.postMessage({ command: 'decode', pages: concat(hdrPages) });
  for (const f of frames) {
    const sent = await new Promise(resolve => {
      const done = m => { enc.removeListener('message', done); resolve(m); };
      enc.on('message', done);
      enc.postMessage({ command: 'encode', buffers: [f] });
    });
    if (sent && sent.message === 'page' && sent.page) {
      dec.postMessage({ command: 'decode', pages: new Uint8Array(sent.page) });
    }
  }
  await new Promise(r => setTimeout(r, 1000));

  const total = out.reduce((s, b) => s + b.length, 0);
  let peak = 0;
  for (const b of out) for (let i = 0; i < b.length; i++) peak = Math.max(peak, Math.abs(b[i]));
  console.log(`decoded blocks: ${out.length}, total samples: ${total}, peak amp: ${peak.toFixed(3)}`);
  const ok = out.length >= 2 && total >= 7000 && peak > 0.05 && peak <= 1.0;
const fs2 = require('fs');
let captured = null;
{
  const w2 = makeWorker('encoderWorker.min.js');
  w2.on('error', e => {});
  const ready2 = new Promise(r => w2.on('message', m => { if (m && m.message === 'ready') r(); }));
  w2.postMessage({ command: 'init', encoderApplication: 2048, encoderFrameSize: 20,
    encoderSampleRate: 16000, originalSampleRate: 48000, numberOfChannels: 1,
    maxFramesPerPage: 1, resampleQuality: 3 });
  ready2.then(() => {
    w2.on('message', m => { if (!captured && m && m.message === 'page' && m.page) {
      captured = Buffer.from(m.page).toString('hex');
      fs2.writeFileSync('/tmp/opus_page.hex', captured);
      w2.terminate();
    }});
    const chunk = new Float32Array(960);
    for (let i = 0; i < chunk.length; i++) chunk[i] = Math.sin(2 * Math.PI * 440 * i / 48000) * 0.5;
    w2.postMessage({ command: 'encode', buffers: [chunk] });
  });
}
setTimeout(() => {
  console.log(ok ? 'OPUS ROUNDTRIP PASSED ✅' : 'OPUS ROUNDTRIP FAILED ❌');
  process.exit(ok ? 0 : 1);
}, 3000);
})();
