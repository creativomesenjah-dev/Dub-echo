// Pruebas offline del núcleo DSP. Compila sin JUCE:
//   g++ -O2 -std=c++17 -I../Source render_test.cpp -o render_test && ./render_test outdir
#include "TapeEchoDSP.h"
#include <chrono>
#include <functional>
#include <cstdio>
#include <string>

using namespace dubecho;
static constexpr int SR = 48000;

static void writeWav (const std::string& path, const std::vector<float>& d)
{
    FILE* f = std::fopen (path.c_str(), "wb");
    if (! f) return;
    const uint32_t dataBytes = (uint32_t) d.size() * 2, riff = 36 + dataBytes, fmtLen = 16, rate = SR, byteRate = SR * 2;
    const uint16_t pcm = 1, ch = 1, align = 2, bits = 16;
    std::fwrite ("RIFF", 1, 4, f); std::fwrite (&riff, 4, 1, f); std::fwrite ("WAVEfmt ", 1, 8, f);
    std::fwrite (&fmtLen, 4, 1, f); std::fwrite (&pcm, 2, 1, f); std::fwrite (&ch, 2, 1, f);
    std::fwrite (&rate, 4, 1, f); std::fwrite (&byteRate, 4, 1, f); std::fwrite (&align, 2, 1, f); std::fwrite (&bits, 2, 1, f);
    std::fwrite ("data", 1, 4, f); std::fwrite (&dataBytes, 4, 1, f);
    for (float s : d) { auto v = (int16_t) std::lround (std::clamp (s, -1.0f, 1.0f) * 32767.0f); std::fwrite (&v, 2, 1, f); }
    std::fclose (f);
}

// Mini loop de dub sintético: bombo, aro y "skank" en el contratiempo. 75 BPM.
static std::vector<float> makeDubLoop (float seconds)
{
    std::vector<float> out ((size_t) (seconds * SR), 0.0f);
    const float beat = 60.0f / 75.0f;
    uint32_t rng = 1;
    auto rnd = [&] { rng = rng * 1664525u + 1013904223u; return (float) (rng >> 8) / 8388608.0f - 1.0f; };
    for (float t0 = 0; t0 < seconds; t0 += beat)
    {
        const int b = (int) std::lround (t0 / beat);
        // bombo en 1 y 3 (one drop simplificado)
        if (b % 2 == 0)
            for (int i = 0; i < SR / 3; ++i)
            {
                const size_t idx = (size_t) (t0 * SR) + i; if (idx >= out.size()) break;
                const float tt = (float) i / SR;
                out[idx] += 0.8f * std::sin (6.2831853f * (55.0f * tt + 40.0f * (1 - std::exp (-tt * 30)) / 30)) * std::exp (-tt * 9);
            }
        // aro en 3
        if (b % 4 == 2)
            for (int i = 0; i < SR / 12; ++i)
            {
                const size_t idx = (size_t) (t0 * SR) + i; if (idx >= out.size()) break;
                out[idx] += 0.35f * rnd() * std::exp (-(float) i / SR * 60);
            }
        // skank en el contratiempo: acorde Am (A3 C4 E4)
        const float ts = t0 + beat * 0.5f;
        for (int i = 0; i < SR / 8; ++i)
        {
            const size_t idx = (size_t) (ts * SR) + i; if (idx >= out.size()) break;
            const float tt = (float) i / SR;
            float s = 0;
            for (float fr : { 220.0f, 261.63f, 329.63f })
                s += 2.0f * (std::fmod (fr * tt, 1.0f) - 0.5f);
            out[idx] += 0.12f * s * std::exp (-tt * 25);
        }
    }
    return out;
}

struct Result { std::vector<float> out; double msPerSec; };

static Result run (TapeEcho& e, const std::vector<float>& in, const std::function<void(int /*block*/, TapeEchoParams&, TapeEcho&)>& automation, TapeEchoParams p)
{
    Result r; r.out.resize (in.size());
    const int bs = 256;
    auto t0 = std::chrono::high_resolution_clock::now();
    for (size_t pos = 0, blk = 0; pos < in.size(); pos += bs, ++blk)
    {
        automation ((int) blk, p, e);
        e.setParams (p);
        const int n = (int) std::min<size_t> (bs, in.size() - pos);
        e.process (in.data() + pos, nullptr, r.out.data() + pos, nullptr, n);
    }
    auto t1 = std::chrono::high_resolution_clock::now();
    r.msPerSec = std::chrono::duration<double, std::milli> (t1 - t0).count() / ((double) in.size() / SR);
    return r;
}

static float peak (const std::vector<float>& v, size_t a, size_t b)
{
    float m = 0; for (size_t i = a; i < std::min (b, v.size()); ++i) m = std::max (m, std::fabs (v[i])); return m;
}
static bool allFinite (const std::vector<float>& v) { for (float s : v) if (! std::isfinite (s)) return false; return true; }

int main (int argc, char** argv)
{
    const std::string dir = argc > 1 ? argv[1] : ".";
    int failures = 0;
    auto check = [&] (bool ok, const char* what) { std::printf ("[%s] %s\n", ok ? " OK " : "FALLA", what); if (! ok) ++failures; };
    const int blocksPerSec = SR / 256;

    // 1) Impulso: el primer eco cae en el tiempo pedido y las repeticiones decaen
    {
        TapeEcho e; TapeEchoParams p; p.timeMs = 600; p.feedback = 0.6f; p.wowFlutter = 0; p.drive = 0.0f;
        e.setParams (p); e.prepare (SR);
        std::vector<float> in ((size_t) SR * 4, 0.0f); in[100] = 0.5f;
        auto r = run (e, in, [] (int, TapeEchoParams&, TapeEcho&) {}, p);
        size_t argmax = 0; float m = 0;
        for (size_t i = 0; i < (size_t) SR; ++i) if (std::fabs (r.out[i]) > m) { m = std::fabs (r.out[i]); argmax = i; }
        const double ms = (double) (argmax - 100) / SR * 1000.0;
        std::printf ("     primer eco a %.2f ms (pedido 600)\n", ms);
        check (std::fabs (ms - 600.0) < 3.0, "tiempo del primer eco preciso");
        const float p1 = peak (r.out, (size_t) (0.55 * SR), (size_t) (0.75 * SR));
        const float p2 = peak (r.out, (size_t) (1.15 * SR), (size_t) (1.35 * SR));
        const float p3 = peak (r.out, (size_t) (1.75 * SR), (size_t) (1.95 * SR));
        std::printf ("     picos repeticiones: %.3f %.3f %.3f\n", p1, p2, p3);
        check (p2 < p1 && p3 < p2, "las repeticiones decaen con feedback 60%");
        check (peak (r.out, 0, (size_t) (0.5 * SR)) < 1e-4f, "nada sale antes del primer eco (100% wet)");
    }

    const auto loop = makeDubLoop (40.0f);
    writeWav (dir + "/00_loop_seco.wav", loop);

    // 2) Uso normal con throws: envío abierto sólo en momentos puntuales
    {
        TapeEcho e; TapeEchoParams p; p.timeMs = 600; p.feedback = 0.62f; p.sendOpen = false;
        e.setParams (p); e.prepare (SR);
        auto r = run (e, loop, [&] (int b, TapeEchoParams& q, TapeEcho&) {
            const float t = (float) b / blocksPerSec;
            q.throwHeld = (t > 1.2f && t < 1.6f) || (t > 7.6f && t < 8.0f) || (t > 14.0f && t < 14.5f);
        }, p);
        std::vector<float> mix (loop.size());
        for (size_t i = 0; i < mix.size(); ++i) mix[i] = 0.7f * loop[i] + 0.7f * r.out[i];
        writeWav (dir + "/01_throws_eco.wav", r.out);
        writeWav (dir + "/01_throws_mezcla.wav", mix);
        check (allFinite (r.out), "throws: sin NaN/inf");
        std::printf ("     CPU: %.3f ms por segundo de audio (%.3f%% de un núcleo)\n", r.msPerSec, r.msPerSec / 10.0);
        check (r.msPerSec < 20.0, "CPU muy por debajo del tiempo real");
    }

    // 3) Autooscilación al 110%: tiene que quedar acotada, no explotar
    {
        TapeEcho e; TapeEchoParams p; p.timeMs = 450; p.feedback = 1.10f; p.sendOpen = false; p.drive = 0.35f;
        e.setParams (p); e.prepare (SR);
        auto r = run (e, loop, [&] (int b, TapeEchoParams& q, TapeEcho&) {
            const float t = (float) b / blocksPerSec;
            q.throwHeld = t > 0.5f && t < 1.5f;
            q.feedback = t < 25.0f ? 1.10f : 0.5f; // a los 25 s se baja el feedback y debe apagarse
        }, p);
        writeWav (dir + "/02_autooscilacion.wav", r.out);
        const float pk = peak (r.out, 0, r.out.size());
        const float late = peak (r.out, (size_t) (20 * SR), (size_t) (25 * SR));
        const float end  = peak (r.out, (size_t) (38 * SR), (size_t) (40 * SR));
        std::printf ("     pico total %.3f | pico 20-25 s %.3f | pico final %.4f\n", pk, late, end);
        check (allFinite (r.out) && pk <= 1.0f, "autooscilación acotada (sin clip digital)");
        check (late > 0.05f, "a 110% el loop se sostiene solo");
        check (end < 0.02f, "al bajar el feedback se apaga");
    }

    // 4) Glide de tiempo: al acortar el tiempo el pitch sube (efecto cinta)
    {
        TapeEcho e; TapeEchoParams p; p.timeMs = 800; p.feedback = 0.0f; p.wowFlutter = 0; p.drive = 0; p.inertiaSec = 0.6f;
        p.lowCutHz = 20; p.highCutHz = 20000;
        e.setParams (p); e.prepare (SR);
        std::vector<float> in ((size_t) SR * 6);
        for (size_t i = 0; i < in.size(); ++i) in[i] = 0.3f * std::sin (6.2831853f * 1000.0f * (float) i / SR);
        auto r = run (e, in, [&] (int b, TapeEchoParams& q, TapeEcho&) {
            if (b == 2 * blocksPerSec) q.timeMs = 200.0f;
        }, p);
        writeWav (dir + "/03_glide_seno.wav", r.out);
        // estimar frecuencia por cruces por cero en ventanas de 50 ms
        auto freqAt = [&] (float sec) {
            const size_t a = (size_t) (sec * SR), b = a + SR / 20; int z = 0;
            for (size_t i = a + 1; i < b; ++i) if ((r.out[i - 1] < 0) != (r.out[i] < 0)) ++z;
            return z / 2.0f / 0.05f;
        };
        const float before = freqAt (1.5f), during = freqAt (2.15f), after = freqAt (5.0f);
        std::printf ("     frecuencia antes %.0f Hz | durante el glide %.0f Hz | después %.0f Hz\n", before, during, after);
        check (std::fabs (before - 1000) < 30 && during > 1300 && std::fabs (after - 1000) < 30, "el glide dobla el pitch y vuelve a afinar");
    }

    // 5) Freeze: se sostiene sin entrada; Kill: corta en <25 ms
    {
        TapeEcho e; TapeEchoParams p; p.timeMs = 600; p.feedback = 0.6f; p.sendOpen = true;
        e.setParams (p); e.prepare (SR);
        auto r = run (e, loop, [&] (int b, TapeEchoParams& q, TapeEcho& eng) {
            const float t = (float) b / blocksPerSec;
            q.sendOpen = t < 3.0f;
            q.freeze = t >= 3.0f && t < 13.0f;
            if (b == 13 * blocksPerSec) { q.freeze = false; q.feedback = 0.6f; }
            if (b == 15 * blocksPerSec) eng.triggerKill();
        }, p);
        writeWav (dir + "/04_freeze_kill.wav", r.out);
        const float early = peak (r.out, (size_t) (4 * SR), (size_t) (5 * SR));
        const float lateF = peak (r.out, (size_t) (12 * SR), (size_t) (13 * SR));
        const float afterKill = peak (r.out, (size_t) (15.03 * SR), (size_t) (16 * SR));
        std::printf ("     freeze: pico inicial %.3f, a los 9 s %.3f | después de kill %.5f\n", early, lateF, afterKill);
        check (lateF > 0.4f * early, "freeze sostiene el loop");
        check (afterKill < 1e-3f, "kill vacía la cinta");
    }

    // 6) Sesión larga (30 min) con wow: estable y sin deriva numérica
    {
        TapeEcho e; TapeEchoParams p; p.timeMs = 500; p.feedback = 0.7f; p.wowFlutter = 1.0f;
        e.setParams (p); e.prepare (SR);
        std::vector<float> in ((size_t) SR * 60 * 30, 0.0f);
        for (size_t k = 0; k < in.size(); k += SR * 7) in[k] = 0.5f;
        auto r = run (e, in, [] (int, TapeEchoParams&, TapeEcho&) {}, p);
        const size_t lastClick = ((in.size() - 1) / (SR * 7)) * (SR * 7);
        size_t argmax = lastClick; float m = 0;
        for (size_t i = lastClick + SR / 4; i < std::min (lastClick + SR, r.out.size()); ++i) if (std::fabs (r.out[i]) > m) { m = std::fabs (r.out[i]); argmax = i; }
        const double ms = (double) (argmax - lastClick) / SR * 1000.0;
        std::printf ("     después de 30 min: eco a %.1f ms (500 ± wow)\n", ms);
        check (allFinite (r.out) && std::fabs (ms - 500) < 10, "estable en sesión larga");
    }

    std::printf ("\n%s (%d fallas)\n", failures == 0 ? "TODO OK" : "HAY FALLAS", failures);
    return failures == 0 ? 0 : 1;
}
