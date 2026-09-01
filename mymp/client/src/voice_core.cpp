/* voice_core.cpp — Opus encode/decode + Ogg page framing (MyMP voice). */
#include "voice_core.h"

#include <opus.h>
#include <cstdio>
#include <cstring>

#define VOICE_RATE 16000
#define VOICE_FRAME_MS 20
#define VOICE_FRAME_SAMPLES (VOICE_RATE * VOICE_FRAME_MS / 1000) /* 320 */
#define OPUS_SERIAL 0x4D594D50u /* 'MYMP' */

/* ---------------- Ogg CRC (poly 0x04c11db7, init 0, no reflection) ------- */
static uint32_t oggCrcTable[256];
static bool oggCrcReady = false;

static void oggCrcInit() {
    for (uint32_t i = 0; i < 256; i++) {
        uint32_t r = i << 24;
        for (int j = 0; j < 8; j++)
            r = (r << 1) ^ ((r & 0x80000000u) ? 0x04c11db7u : 0);
        oggCrcTable[i] = r;
    }
    oggCrcReady = true;
}

static uint32_t oggCrc(const uint8_t* data, size_t len) {
    if (!oggCrcReady) oggCrcInit();
    uint32_t crc = 0;
    for (size_t i = 0; i < len; i++)
        crc = (crc << 8) ^ oggCrcTable[((crc >> 24) & 0xFF) ^ data[i]];
    return crc;
}

static void putLE32(uint8_t* p, uint32_t v) {
    p[0] = (uint8_t)(v); p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24);
}
static void putLE64(uint8_t* p, uint64_t v) {
    for (int i = 0; i < 8; i++) p[i] = (uint8_t)(v >> (8 * i));
}

/* Build one Ogg page from a packet. */
static void buildOggPage(uint8_t headerType, uint64_t granule,
                         uint32_t pageSeq, const uint8_t* packet, size_t packetLen,
                         std::string& out) {
    out.clear();
    out.resize(27 + 1 + packetLen); /* 27 header + 1 segment + payload */
    uint8_t* p = (uint8_t*)&out[0];
    memcpy(p, "OggS", 4);
    p[4] = 0;                          /* version */
    p[5] = headerType;                 /* 2=BOS 0=normal 4=EOS */
    putLE64(p + 6, granule);
    putLE32(p + 14, OPUS_SERIAL);
    putLE32(p + 18, pageSeq);
    putLE32(p + 22, 0);                /* checksum, patched below */
    p[26] = 1;                         /* one segment */
    p[27] = (uint8_t)packetLen;
    memcpy(p + 28, packet, packetLen);
    putLE32(p + 22, oggCrc(p, out.size()));
}

/* ---------------- OpusHead / OpusTags packets ---------------- */
static void putLE16(uint8_t* p, uint16_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); }

static void makeOpusHead(std::string& out) {
    out.resize(19);
    uint8_t* p = (uint8_t*)&out[0];
    memcpy(p, "OpusHead", 8);
    p[8] = 1;                    /* version */
    p[9] = 1;                    /* channels (mono) */
    putLE16(p + 10, 0);          /* pre-skip */
    putLE32(p + 12, 48000);      /* original input sample rate */
    putLE16(p + 16, 0);          /* output gain */
    p[18] = 0;                   /* mapping family */
}
static void makeOpusTags(std::string& out) {
    out.resize(8 + 4 + 4 + 4);
    uint8_t* p = (uint8_t*)&out[0];
    memcpy(p, "OpusTags", 8);
    putLE32(p + 8, 4);                       /* vendor string length */
    memcpy(p + 12, "MyMP", 4);
    putLE32(p + 16, 0);                      /* zero user comments */
}

/* ---------------- encoder ---------------- */
struct VoiceEncoder {
    OpusEncoder* op;
    uint64_t granule;        /* total encoded samples (16 kHz) */
    uint32_t pageSeq;        /* 0 = OpusHead, 1 = OpusTags, 2+ = audio */
    bool headersSent;
};

bool voiceEncoderCreate(VoiceEncoder** out) {
    VoiceEncoder* v = new VoiceEncoder();
    int err = 0;
    v->op = opus_encoder_create(VOICE_RATE, 1, OPUS_APPLICATION_VOIP, &err);
    if (err != OPUS_OK || !v->op) { delete v; return false; }
    opus_encoder_ctl(v->op, OPUS_SET_BITRATE(20000));
    opus_encoder_ctl(v->op, OPUS_SET_COMPLEXITY(6));
    v->granule = 0;
    v->pageSeq = 0;
    v->headersSent = false;
    *out = v;
    return true;
}

void voiceEncoderFree(VoiceEncoder* v) {
    if (!v) return;
    if (v->op) opus_encoder_destroy(v->op);
    delete v;
}

void voiceHeaderPages(VoiceEncoder* v, std::string& idPage, std::string& tagPage) {
    std::string head, tags;
    makeOpusHead(head);
    makeOpusTags(tags);
    buildOggPage(2, 0, 0, (const uint8_t*)head.data(), head.size(), idPage);
    buildOggPage(0, 0, 1, (const uint8_t*)tags.data(), tags.size(), tagPage);
    v->pageSeq = 2;
}

bool voiceEncodePage(VoiceEncoder* v, const float* pcm320, std::string& pageOut) {
    uint8_t frame[1500];
    int n = opus_encode_float(v->op, pcm320, VOICE_FRAME_SAMPLES, frame, sizeof frame);
    if (n <= 0) return false;
    v->granule += VOICE_FRAME_SAMPLES;
    buildOggPage(0, v->granule, v->pageSeq++, frame, (size_t)n, pageOut);
    return true;
}

void voiceEncodeBuffer(VoiceEncoder* v, const float* pcm16k, int nSamples,
                       std::vector<std::string>& pagesOut) {
    static const int F = VOICE_FRAME_SAMPLES;
    for (int off = 0; off + F <= nSamples; off += F) {
        std::string page;
        if (voiceEncodePage(v, pcm16k + off, page)) pagesOut.push_back(std::move(page));
    }
}

/* ---------------- decoder ---------------- */
struct VoiceDecoder {
    OpusDecoder* op;
    bool inited;
    std::vector<uint8_t> preBOS;   /* pages buffered until the BOS arrives */
};

bool voiceDecoderCreate(VoiceDecoder** out) {
    VoiceDecoder* v = new VoiceDecoder();
    int err = 0;
    v->op = opus_decoder_create(VOICE_RATE, 1, &err);
    if (err != OPUS_OK || !v->op) { delete v; return false; }
    v->inited = false;
    *out = v;
    return true;
}

void voiceDecoderFree(VoiceDecoder* v) {
    if (!v) return;
    if (v->op) opus_decoder_destroy(v->op);
    delete v;
}

bool voicePageIsOgg(const uint8_t* page, size_t len) {
    return len >= 28 && memcmp(page, "OggS", 4) == 0;
}
bool voicePageIsBOS(const uint8_t* page, size_t len) {
    return voicePageIsOgg(page, len) && (page[5] & 2) != 0;
}

/* Find Ogg page boundaries inside a blob. */
static void findPages(const uint8_t* data, size_t len,
                      std::vector<std::pair<const uint8_t*, size_t>>& out) {
    for (size_t i = 0; i + 27 <= len; i++) {
        if (memcmp(data + i, "OggS", 4) != 0) continue;
        size_t segs = data[i + 26];
        if (i + 27 + segs > len) continue;
        size_t body = 0;
        for (size_t s = 0; s < segs; s++) body += data[i + 27 + s];
        size_t end = i + 27 + segs + body;
        if (end <= len) out.emplace_back(data + i, end - i);
    }
}

bool voiceDecodePage(VoiceDecoder* v, const uint8_t* page, size_t len,
                     std::vector<float>& pcmOut) {
    if (!voicePageIsOgg(page, len)) return false;
    if (voicePageIsBOS(page, len)) {
        v->inited = true;
        /* flush anything buffered before the header arrived */
        if (!v->preBOS.empty()) {
            std::vector<std::pair<const uint8_t*, size_t>> pgs;
            findPages(v->preBOS.data(), v->preBOS.size(), pgs);
            for (auto& pg : pgs) voiceDecodePage(v, pg.first, pg.second, pcmOut);
        }
        v->preBOS.clear();
        return false;
    }
    if (!v->inited) {
        /* late join: hold audio pages until the 5s header re-send arrives */
        if (v->preBOS.size() + len < 16384)
            v->preBOS.insert(v->preBOS.end(), page, page + len);
        return false;
    }
    /* page index: 0/1 are headers (never decoded), audio starts at 2 */
    uint32_t pageIndex = (uint32_t)page[18] | ((uint32_t)page[19] << 8) |
                         ((uint32_t)page[20] << 16) | ((uint32_t)page[21] << 24);
    if (pageIndex < 2) return false;

    /* parse segments -> packets; decode each complete packet */
    size_t segs = page[26];
    size_t pos = 27 + segs;
    size_t packetLen = 0;
    float outbuf[960 * 2];
    bool any = false;
    for (size_t s = 0; s < segs; s++) {
        uint8_t lace = page[27 + s];
        packetLen += lace;
        if (lace < 255) {              /* packet complete */
            int n = opus_decode_float(v->op, page + pos, (opus_int32)packetLen,
                                      outbuf, 960, 0);
            pos += packetLen;
            packetLen = 0;
            if (n > 0) {
                pcmOut.insert(pcmOut.end(), outbuf, outbuf + n);
                any = true;
            }
        }
    }
    return any;
}
