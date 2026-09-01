/* voice_audio.cpp — WASAPI capture/playback for MyMP in-game voice.
 *
 * Capture: default communications mic, shared mode, event callback.
 *   mono mix -> linear resample to 16 kHz -> voice core (Opus/Ogg) ->
 *   send callback [0x56][0x4F][page] when the PTT key is held.
 *
 * Render: default communications speakers, shared mode, event callback.
 *   incoming voice datagrams are decoded per speaker (sid) into 16 kHz mono
 *   queues; the render loop mixes them and resamples to the device rate.
 */
#include "voice_audio.h"
#include "voice_core.h"

#ifdef _WIN32

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <mmsystem.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <deque>
#include <map>
#include <mutex>
#include <thread>
#include <vector>

/* ---- GUIDs (explicit, avoids __uuidof / uuid.lib differences) ---- */
static const GUID G_CLSID_MMDeviceEnumerator =
    {0xBCDE0395, 0xE52F, 0x467C, {0x8E, 0x3D, 0xC4, 0x57, 0x92, 0x91, 0x69, 0x2E}};
static const GUID G_IID_IMMDeviceEnumerator =
    {0xA95664D2, 0x9614, 0x4F35, {0xA7, 0x46, 0xDE, 0x8D, 0xB6, 0x36, 0x17, 0xE6}};
static const GUID G_IID_IAudioClient =
    {0x1CB9AD4C, 0xDBFA, 0x4C32, {0xB1, 0x78, 0xC2, 0xF5, 0x68, 0xA7, 0x03, 0xB2}};
static const GUID G_IID_IAudioCaptureClient =
    {0xC8ADBD64, 0xE71E, 0x48A0, {0xA4, 0xDE, 0x18, 0x5C, 0x39, 0x5C, 0xD3, 0x17}};
static const GUID G_IID_IAudioRenderClient =
    {0xF294ACFC, 0x3146, 0x4483, {0xA7, 0xBF, 0xAD, 0xDC, 0xA7, 0xC2, 0x60, 0xE2}};
static const GUID G_KSDATAFORMAT_SUBTYPE_IEEE_FLOAT =
    {0x00000003, 0x0000, 0x0010, {0x80, 0x00, 0x00, 0xAA, 0x00, 0x38, 0x9B, 0x71}};

static bool isFloatFormat(WAVEFORMATEX* wf) {
    if (wf->wFormatTag == WAVE_FORMAT_IEEE_FLOAT) return true;
    if (wf->wFormatTag == WAVE_FORMAT_EXTENSIBLE && wf->cbSize >= 22) {
        WAVEFORMATEXTENSIBLE* we = (WAVEFORMATEXTENSIBLE*)wf;
        return memcmp(&we->SubFormat, &G_KSDATAFORMAT_SUBTYPE_IEEE_FLOAT,
                      sizeof(GUID)) == 0;
    }
    return false;
}

/* ---------- linear resampler ---------- */
class Resampler {
public:
    void init(double inRate, double outRate) { ratio = inRate / outRate; pos = 0; }
    void push(const float* s, int n) { buf.insert(buf.end(), s, s + n); }
    /* pull up to maxN output samples; returns count written */
    int pull(float* out, int maxN) {
        int n = 0;
        while (n < maxN && (size_t)(pos + 1) < buf.size()) {
            size_t i = (size_t)pos;
            float frac = (float)(pos - i);
            out[n++] = buf[i] * (1.0f - frac) + buf[i + 1] * frac;
            pos += ratio;
        }
        if (n > 0) {
            size_t drop = (size_t)pos;
            pos -= (double)drop;
            buf.erase(buf.begin(), buf.begin() + (long)drop);
        }
        return n;
    }
    bool starved() const { return (size_t)(pos + 1) >= buf.size(); }
    void clear() { buf.clear(); pos = 0; }
private:
    std::vector<float> buf;
    double ratio = 3.0, pos = 0.0;
};

/* ---------- global voice state ---------- */
static void (*g_send)(const uint8_t*, size_t) = nullptr;
static bool g_active = false;

static std::mutex g_micMutex;
static std::thread g_capThread;
static std::thread g_renThread;
static HANDLE g_capEvent = nullptr, g_renEvent = nullptr;
static volatile bool g_running = false;
static volatile bool g_talking = false;
static bool g_micGotData = false;

static std::mutex g_speakerMutex;
struct Speaker {
    VoiceDecoder* dec = nullptr;
    std::deque<float> pcm;      /* decoded 16 kHz mono */
};
static std::map<uint32_t, Speaker> g_speakers;

/* ---------- capture: device setup + loop ---------- */
struct AudioClientPair { IMMDevice* dev; IAudioClient* ac; };

static bool openAudioClient(EDataFlow flow, IAudioClient** acOut, WAVEFORMATEX** fmtOut) {
    IMMDeviceEnumerator* enumerator = nullptr;
    HRESULT hr = CoCreateInstance(G_CLSID_MMDeviceEnumerator, nullptr, CLSCTX_ALL,
                                  G_IID_IMMDeviceEnumerator, (void**)&enumerator);
    if (FAILED(hr) || !enumerator) return false;
    IMMDevice* device = nullptr;
    hr = enumerator->GetDefaultAudioEndpoint(flow, eCommunications, &device);
    enumerator->Release();
    if (FAILED(hr) || !device) return false;
    IAudioClient* ac = nullptr;
    hr = device->Activate(G_IID_IAudioClient, CLSCTX_ALL, nullptr, (void**)&ac);
    device->Release();
    if (FAILED(hr) || !ac) return false;
    WAVEFORMATEX* fmt = nullptr;
    hr = ac->GetMixFormat(&fmt);
    if (FAILED(hr) || !fmt) { ac->Release(); return false; }
    *acOut = ac;
    *fmtOut = fmt;
    return true;
}

static void captureLoop() {
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    IAudioClient* ac = nullptr;
    IAudioCaptureClient* cc = nullptr;
    WAVEFORMATEX* fmt = nullptr;
    if (!openAudioClient(eCapture, &ac, &fmt)) { CoUninitialize(); return; }

    int inRate = fmt->nSamplesPerSec;
    bool isFloat = isFloatFormat(fmt);
    int chans = fmt->nChannels;
    bool ok = SUCCEEDED(ac->Initialize(AUDCLNT_SHAREMODE_SHARED,
                                       AUDCLNT_STREAMFLAGS_EVENTCALLBACK,
                                       500000, 0, fmt, nullptr));
    CoTaskMemFree(fmt);
    if (!ok || FAILED(ac->GetService(G_IID_IAudioCaptureClient, (void**)&cc)) || !cc) {
        ac->Release(); CoUninitialize(); return;
    }
    g_capEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    ac->SetEventHandle(g_capEvent);
    if (FAILED(ac->Start())) { cc->Release(); ac->Release(); CoUninitialize(); return; }

    VoiceEncoder* enc = nullptr;
    voiceEncoderCreate(&enc);
    std::string idPage, tagPage;
    if (enc) voiceHeaderPages(enc, idPage, tagPage);
    Resampler rs;
    rs.init(inRate, 16000);
    float inBuf[2048];
    bool sentHeader = false;
    int headerCountdown = 0;

    while (g_running) {
        DWORD w = WaitForSingleObject(g_capEvent, 300);
        if (w != WAIT_OBJECT_0) continue;
        UINT32 packets = 0;
        while (SUCCEEDED(cc->GetNextPacketSize(&packets)) && packets > 0) {
            BYTE* data = nullptr;
            UINT32 frames = 0;
            DWORD flags = 0;
            if (FAILED(cc->GetBuffer(&data, &frames, &flags, nullptr, nullptr)) || !frames)
                break;
            if (!(flags & AUDCLNT_BUFFERFLAGS_SILENT)) {
                /* mono mix + push to resampler */
                float mono[1024];
                int n = (int)frames < 1024 ? (int)frames : 1024;
                if (isFloat) {
                    const float* src = (const float*)data;
                    for (int i = 0; i < n; i++) {
                        float s = 0;
                        for (int c = 0; c < chans; c++) s += src[i * chans + c];
                        mono[i] = s / chans;
                    }
                } else {
                    const int16_t* src = (const int16_t*)data;
                    for (int i = 0; i < n; i++) {
                        float s = 0;
                        for (int c = 0; c < chans; c++) s += src[i * chans + c];
                        mono[i] = s / (chans * 32768.0f);
                    }
                }
                rs.push(mono, n);
                g_micGotData = true;
            }
            cc->ReleaseBuffer(frames);

            /* encode what we can (20 ms chunks) while PTT held */
            if (g_talking && g_send && enc) {
                if (!sentHeader) {
                    std::string hdr = "\x56\x4F";      /* voice datagram + opus marker */
                    hdr.append(idPage);
                    g_send((const uint8_t*)hdr.data(), hdr.size());
                    std::string hdr2 = "\x56\x4F";
                    hdr2.append(tagPage);
                    g_send((const uint8_t*)hdr2.data(), hdr2.size());
                    sentHeader = true;
                    headerCountdown = 500;              /* ~5 s at ~100 events/s */
                }
                float pcm16k[640];
                int got = rs.pull(pcm16k, 640);
                std::vector<std::string> pages;
                if (got >= 320) voiceEncodeBuffer(enc, pcm16k, got, pages);
                for (auto& pg : pages) {
                    std::string out = "\x56\x4F";
                    out.append(pg);
                    g_send((const uint8_t*)out.data(), out.size());
                }
                if (--headerCountdown <= 0 && g_talking) {
                    std::string hdr = "\x56\x4F";
                    hdr.append(idPage);
                    g_send((const uint8_t*)hdr.data(), hdr.size());
                    std::string hdr2 = "\x56\x4F";
                    hdr2.append(tagPage);
                    g_send((const uint8_t*)hdr2.data(), hdr2.size());
                    headerCountdown = 500;
                }
            }
        }
    }
    if (enc) { voiceEncoderFree(enc); enc = nullptr; }
    ac->Stop();
    if (cc) cc->Release();
    ac->Release();
    if (g_capEvent) { CloseHandle(g_capEvent); g_capEvent = nullptr; }
    CoUninitialize();
}

/* ---------- render: decode + mix + WASAPI out ---------- */
static void renderLoop() {
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    IAudioClient* ac = nullptr;
    IAudioRenderClient* rc = nullptr;
    WAVEFORMATEX* fmt = nullptr;
    if (!openAudioClient(eRender, &ac, &fmt)) { CoUninitialize(); return; }

    int outRate = fmt->nSamplesPerSec;
    bool isFloat = isFloatFormat(fmt);
    int chans = fmt->nChannels > 0 ? fmt->nChannels : 2;
    bool ok = SUCCEEDED(ac->Initialize(AUDCLNT_SHAREMODE_SHARED,
                                       AUDCLNT_STREAMFLAGS_EVENTCALLBACK,
                                       500000, 0, fmt, nullptr));
    CoTaskMemFree(fmt);
    if (!ok || FAILED(ac->GetService(G_IID_IAudioRenderClient, (void**)&rc)) || !rc) {
        ac->Release(); CoUninitialize(); return;
    }
    UINT32 bufferFrames = 0;
    ac->GetBufferSize(&bufferFrames);
    g_renEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    ac->SetEventHandle(g_renEvent);
    if (FAILED(ac->Start())) { rc->Release(); ac->Release(); CoUninitialize(); return; }

    Resampler rs;
    rs.init(16000, (double)outRate);
    float mixed[960];          /* one 16 kHz 60 ms block */
    const double FRAMES_16K = 16000.0 / outRate;

    while (g_running) {
        DWORD w = WaitForSingleObject(g_renEvent, 300);
        UINT32 padding = 0;
        ac->GetCurrentPadding(&padding);
        UINT32 avail = bufferFrames > padding ? bufferFrames - padding : 0;
        if (w == WAIT_OBJECT_0 && avail > 0) {
            BYTE* buf = nullptr;
            if (SUCCEEDED(rc->GetBuffer(avail, &buf))) {
                for (UINT32 f = 0; f < avail; f++) {
                    float s16k = 0;
                    /* refill the 16 kHz mixer stream in 960-sample blocks,
                       then let the resampler produce this frame */
                    if (rs.starved()) {
                        std::lock_guard<std::mutex> lk(g_speakerMutex);
                        size_t nSpeakers = g_speakers.size();
                        for (int i = 0; i < 960; i++) {
                            float s = 0;
                            for (auto& kv : g_speakers) {
                                auto& q = kv.second.pcm;
                                if (!q.empty()) { s += q.front(); q.pop_front(); }
                            }
                            mixed[i] = (nSpeakers ? s / (float)nSpeakers : 0.0f);
                        }
                        rs.push(mixed, 960);
                    }
                    rs.pull(&s16k, 1);
                    if (s16k > 1.0f) s16k = 1.0f;
                    if (s16k < -1.0f) s16k = -1.0f;
                    if (isFloat) {
                        float* out = (float*)buf + (size_t)f * chans;
                        for (int c = 0; c < chans; c++) out[c] = s16k;
                    } else {
                        int16_t v = (int16_t)(s16k * 32767.0f);
                        int16_t* out = (int16_t*)buf + (size_t)f * chans;
                        for (int c = 0; c < chans; c++) out[c] = v;
                    }
                }
                rc->ReleaseBuffer(avail, 0);
            }
        }
    }
    ac->Stop();
    if (rc) rc->Release();
    ac->Release();
    if (g_renEvent) { CloseHandle(g_renEvent); g_renEvent = nullptr; }
    CoUninitialize();
}

/* ---------- public API ---------- */
void voiceAudioInit(void (*sendFn)(const uint8_t*, size_t)) {
    if (g_running) return;
    g_send = sendFn;
    g_running = true;
    g_talking = false;
    g_micGotData = false;
    g_capThread = std::thread(captureLoop);
    g_renThread = std::thread(renderLoop);
    g_active = true;
}

void voiceAudioShutdown() {
    if (!g_running) return;
    g_running = false;
    if (g_capThread.joinable()) g_capThread.join();
    if (g_renThread.joinable()) g_renThread.join();
    std::lock_guard<std::mutex> lk(g_speakerMutex);
    for (auto& kv : g_speakers) {
        if (kv.second.dec) voiceDecoderFree(kv.second.dec);
    }
    g_speakers.clear();
    g_active = false;
    g_send = nullptr;
}

void voiceAudioSetTalking(bool talking) { g_talking = talking; }

void voiceAudioFeed(const uint8_t* data, size_t len) {
    if (len < 6) return;
    uint32_t sid = (uint32_t)data[0] | ((uint32_t)data[1] << 8) |
                   ((uint32_t)data[2] << 16) | ((uint32_t)data[3] << 24);
    float vol = data[4] / 255.0f;
    const uint8_t* payload = data + 5;
    size_t plen = len - 5;
    std::lock_guard<std::mutex> lk(g_speakerMutex);
    Speaker& sp = g_speakers[sid];
    if (!sp.dec) voiceDecoderCreate(&sp.dec);
    if (!sp.dec) return;
    /* eviction: more than 12 speakers, drop the oldest-idle */
    if (g_speakers.size() > 12) {
        auto it = g_speakers.begin();
        if (it->first != sid) g_speakers.erase(it);
    }
    std::vector<float> pcm;
    if (plen > 2 && payload[0] == 0x4F) {
        voiceDecodePage(sp.dec, payload + 1, plen - 1, pcm);
    } else if (plen >= 640) {
        /* browser raw-PCM fallback: 16 kHz int16 */
        int n = (int)(plen / 2);
        pcm.resize(n);
        for (int i = 0; i < n; i++) {
            int16_t v = (int16_t)(payload[i * 2] | (payload[i * 2 + 1] << 8));
            pcm[i] = v / 32768.0f;
        }
    }
    if (!pcm.empty()) {
        if (vol < 1.0f)
            for (float& x : pcm) x *= vol;
        if (sp.pcm.size() + pcm.size() > 24000)  /* >1.5 s buffer: drop oldest */
            sp.pcm.erase(sp.pcm.begin(), sp.pcm.begin() + (long)pcm.size());
        sp.pcm.insert(sp.pcm.end(), pcm.begin(), pcm.end());
    }
}

bool voiceAudioActive() { return g_active; }

#else  /* !_WIN32 — stub for the Linux test build */
void voiceAudioInit(void (*)(const uint8_t*, size_t)) {}
void voiceAudioShutdown() {}
void voiceAudioSetTalking(bool) {}
void voiceAudioFeed(const uint8_t*, size_t) {}
bool voiceAudioActive() { return false; }
#endif
