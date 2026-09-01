/* voice_audio.h — MyMP in-game voice: WASAPI capture + playback (Windows).
 * The capture thread encodes 16 kHz mono PCM with the voice core (Opus ->
 * single-packet Ogg pages) and hands them to a send callback; the render
 * thread mixes decoded speakers and plays via WASAPI.
 */
#pragma once
#include <cstdint>
#include <cstddef>

/* sendFn receives the full UDP voice datagram: [0x56][0x4F][ogg-page...] */
void voiceAudioInit(void (*sendFn)(const uint8_t*, size_t));
void voiceAudioShutdown();

/* PTT: call every frame with whether the talk key is held */
void voiceAudioSetTalking(bool talking);

/* Incoming voice datagram from the server:
 * [0x56][sid:4][vol:1][payload] — payload is [0x4F][ogg page] (opus) or
 * raw 16 kHz int16 PCM (browser fallback). */
void voiceAudioFeed(const uint8_t* data, size_t len);

/* was the mic ever captured (for HUD hint)? */
bool voiceAudioActive();
