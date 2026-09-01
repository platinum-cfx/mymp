/* voice_core.h — MyMP in-game voice: Opus codec + Ogg framing.
 *
 * Platform-neutral core (compiles on Linux for tests AND Windows for the
 * ASI). The wire format matches the browser voice exactly (opus-recorder):
 * every 20 ms voice frame is a single-packet Ogg page (OpusHead BOS page +
 * OpusTags comment page once per stream), so GTA V players and browser
 * players can hear each other, and the same server routes both.
 */
#pragma once
#include <cstdint>
#include <cstddef>
#include <deque>
#include <string>
#include <vector>

/* ---------- one encoder instance (per talking client) ---------- */
struct VoiceEncoder;
bool  voiceEncoderCreate(VoiceEncoder** out);
void  voiceEncoderFree(VoiceEncoder* v);
/* 20 ms mono 16 kHz PCM (320 floats) -> one Ogg audio page */
bool  voiceEncodePage(VoiceEncoder* v, const float* pcm320, std::string& pageOut);
/* OpusHead (BOS) + OpusTags (comment) pages — send once at stream start and
   periodically (receivers init their decoder on the BOS page) */
void  voiceHeaderPages(VoiceEncoder* v, std::string& idPage, std::string& tagPage);
/* 16 kHz mono float input -> 20 ms frame, used by the WASAPI resampler */
void  voiceEncodeBuffer(VoiceEncoder* v, const float* pcm16k, int nSamples,
                        std::vector<std::string>& pagesOut);

/* ---------- one decoder instance (per heard speaker) ---------- */
struct VoiceDecoder;
bool  voiceDecoderCreate(VoiceDecoder** out);
void  voiceDecoderFree(VoiceDecoder* v);
/* Feed one page (header or audio). When decodable audio results, append
 * 16 kHz mono float PCM to pcmOut. Returns true if pcmOut was appended. */
bool  voiceDecodePage(VoiceDecoder* v, const uint8_t* page, size_t len,
                      std::vector<float>& pcmOut);

/* ---------- helper: is this page the Ogg beginning-of-stream (OpusHead)? ---------- */
bool voicePageIsBOS(const uint8_t* page, size_t len);
bool voicePageIsOgg(const uint8_t* page, size_t len);
