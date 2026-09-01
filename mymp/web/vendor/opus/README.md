# web/vendor/opus — test fixture (NOT a client)

These files (opus-recorder, MIT, by Chris Rudmin) are used **only** by the
voice-compatibility tests in `tests/` (`opus_roundtrip.js`,
`native_browser_voice.js`) as an independent reference decoder that validates
the GTA client's Ogg/Opus framing.

MyMP is GTA V only (like FiveM) — there is no browser game client, and the
GTA client ships its own bundled libopus (BSD-3) inside MyMP.asi.
