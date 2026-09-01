/* voice_core_test.cpp — Linux harness for MyMP voice core.
 * 1) encode a 440 Hz sine into Ogg pages (headers + audio)
 * 2) decode the pages back (native decoder)
 * 3) dump the pages to /tmp so tests/opus_roundtrip.js can feed them to the
 *    BROWSER decoder worker — proving GTA V players and browser players
 *    use a byte-compatible voice stream.
 */
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include "voice_core.h"

int main() {
    VoiceEncoder* enc = nullptr;
    if (!voiceEncoderCreate(&enc)) { printf("encoder create FAILED\n"); return 1; }
    VoiceDecoder* dec = nullptr;
    if (!voiceDecoderCreate(&dec)) { printf("decoder create FAILED\n"); return 1; }

    std::string idPage, tagPage;
    voiceHeaderPages(enc, idPage, tagPage);

    FILE* pages = fopen("/tmp/native_pages.bin", "wb");
    if (!pages) { printf("cannot write /tmp/native_pages.bin\n"); return 1; }
    fwrite(idPage.data(), 1, idPage.size(), pages);
    fwrite(tagPage.data(), 1, tagPage.size(), pages);
    fclose(pages);

    // 1 s of 440 Hz sine @ 16 kHz = 50 frames
    float pcm[320];
    std::vector<float> decoded;
    std::vector<std::string> pagesVec;
    for (int f = 0; f < 50; f++) {
        for (int i = 0; i < 320; i++)
            pcm[i] = 0.5f * std::sin(2.0 * M_PI * 440.0 * (f * 320 + i) / 16000.0);
        std::string page;
        if (!voiceEncodePage(enc, pcm, page)) { printf("encode FAILED at frame %d\n", f); return 1; }
        pagesVec.push_back(page);
    }
    FILE* audio = fopen("/tmp/native_pages.bin", "ab");
    for (auto& p : pagesVec) fwrite(p.data(), 1, p.size(), audio);
    fclose(audio);

    // decode: feed header pages then audio pages
    voiceDecodePage(dec, (const uint8_t*)idPage.data(), idPage.size(), decoded);
    voiceDecodePage(dec, (const uint8_t*)tagPage.data(), tagPage.size(), decoded);
    for (auto& p : pagesVec)
        voiceDecodePage(dec, (const uint8_t*)p.data(), p.size(), decoded);

    // also test the late-join path: fresh decoder gets audio BEFORE headers
    VoiceDecoder* dec2 = nullptr;
    voiceDecoderCreate(&dec2);
    std::vector<float> late;
    for (auto& p : pagesVec)
        voiceDecodePage(dec2, (const uint8_t*)p.data(), p.size(), late); // buffered
    voiceDecodePage(dec2, (const uint8_t*)idPage.data(), idPage.size(), late); // BOS flushes
    voiceDecodePage(dec2, (const uint8_t*)tagPage.data(), tagPage.size(), late);

    float peak = 0;
    for (float x : decoded) peak = std::max(peak, std::fabs(x));
    float peakLate = 0;
    for (float x : late) peakLate = std::max(peakLate, std::fabs(x));
    printf("native decode: %zu samples, peak %.3f\n", decoded.size(), peak);
    printf("late-join path: %zu samples, peak %.3f\n", late.size(), peakLate);

    bool ok = decoded.size() >= 15000 && peak > 0.2 && peak <= 1.01 &&
              late.size() >= 10000 && peakLate > 0.2;
    printf(ok ? "VOICE CORE TEST PASSED\n" : "VOICE CORE TEST FAILED\n");
    voiceEncoderFree(enc);
    voiceDecoderFree(dec);
    voiceDecoderFree(dec2);
    return ok ? 0 : 1;
}
