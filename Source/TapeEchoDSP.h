// TapeEchoDSP.h
// Núcleo DSP del Dub Echo: eco de cinta estilo dub, sin dependencias de JUCE.
// Pensado para ir en un envío paralelo: entra la música, sale SOLO el eco (100% wet).
//
// Cadena por muestra:
//   entrada (mono) -> compuerta de envío (throw / envío continuo)
//   -> cabeza de grabación: saturación de cinta (entrada + realimentación)
//   -> línea de retardo con tiempo "con inercia" (cambiar el tiempo dobla el pitch, como la cinta)
//      + wow & flutter
//   -> cabeza de reproducción -> filtros del loop (graves / agudos) -> salida y realimentación
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace dubecho
{

// Filtro de estado variable (TPT, Zavalishin): estable al modular la frecuencia.
struct SVF
{
    float ic1 = 0.0f, ic2 = 0.0f;
    float a1 = 0.0f, a2 = 0.0f, a3 = 0.0f, k = 1.41421356f;

    void reset() { ic1 = ic2 = 0.0f; }

    void setCutoff (float hz, float sampleRate, float q = 0.70710678f)
    {
        hz = std::clamp (hz, 10.0f, 0.45f * sampleRate);
        const float g = std::tan (3.14159265f * hz / sampleRate);
        k  = 1.0f / q;
        a1 = 1.0f / (1.0f + g * (g + k));
        a2 = g * a1;
        a3 = g * a2;
    }

    // Devuelve pasa-bajos y pasa-altos en una sola pasada.
    inline void process (float x, float& low, float& high)
    {
        const float v3 = x - ic2;
        const float v1 = a1 * ic1 + a2 * v3;
        const float v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.0f * v1 - ic1;
        ic2 = 2.0f * v2 - ic2;
        low  = v2;
        high = x - k * v1 - v2;
    }
};

struct OnePole
{
    float y = 0.0f, coef = 0.0f;
    void setTime (float seconds, float sampleRate)
    {
        coef = seconds <= 0.0f ? 0.0f : std::exp (-1.0f / (seconds * sampleRate));
    }
    inline float process (float target) { y = target + coef * (y - target); return y; }
    void snap (float v) { y = v; }
};

// Parámetros "de usuario". Se copian al DSP una vez por bloque.
struct TapeEchoParams
{
    float timeMs      = 375.0f;  // 20 .. 2000
    float feedback    = 0.55f;   // 0 .. 1.10  (>1 = autooscilación contenida por la saturación)
    float inertiaSec  = 0.35f;   // 0.02 .. 2  (cuánto tarda la "cinta" en llegar al nuevo tiempo)
    float wowFlutter  = 0.25f;   // 0 .. 1
    float drive       = 0.35f;   // 0 .. 1     (saturación de cinta)
    float lowCutHz    = 180.0f;  // pasa-altos dentro del loop
    float highCutHz   = 3200.0f; // pasa-bajos dentro del loop
    float inputGain   = 1.0f;    // lineal
    float outputGain  = 1.0f;    // lineal
    bool  sendOpen    = true;    // envío continuo
    bool  throwHeld   = false;   // throw momentáneo
    bool  freeze      = false;   // loop congelado (sin entrada, realimentación al 100%)
};

class TapeEcho
{
public:
    static constexpr float kMinTimeMs = 20.0f;
    static constexpr float kMaxTimeMs = 2000.0f;
    static constexpr float kMaxFeedback = 1.10f;

    void prepare (double sampleRate)
    {
        sr = (float) sampleRate;

        const auto maxSamples = (size_t) std::ceil ((kMaxTimeMs + 30.0f) * 0.001f * sr) + 16;
        size_t size = 1;
        while (size < maxSamples) size <<= 1;
        buffer.assign (size, 0.0f);
        mask = size - 1;
        writeIdx = 0;

        delaySmp = params.timeMs * 0.001f * sr;

        gateSmooth.setTime (0.004f, sr);
        fbSmooth.setTime (0.03f, sr);
        inSmooth.setTime (0.01f, sr);
        outSmooth.setTime (0.01f, sr);
        filterMix.setTime (0.05f, sr);
        lowCutSmooth.setTime (0.03f, sr);
        highCutSmooth.setTime (0.03f, sr);

        gateSmooth.snap (0.0f);
        fbSmooth.snap (params.feedback);
        inSmooth.snap (params.inputGain);
        outSmooth.snap (params.outputGain);
        filterMix.snap (1.0f);
        lowCutSmooth.snap (params.lowCutHz);
        highCutSmooth.snap (params.highCutHz);

        reset();
    }

    void reset()
    {
        std::fill (buffer.begin(), buffer.end(), 0.0f);
        hp.reset(); lp.reset();
        dcX = dcY = 0.0f;
        killGain = 1.0f;
        killing = false;
        wowPhase = flutterPhase = 0.0f;
        noiseValue = noiseTarget = 0.0f;
        noiseCounter = 0;
    }

    void setParams (const TapeEchoParams& p) { params = p; }

    // Corta todas las repeticiones (fade de 20 ms y vaciado de la cinta).
    void triggerKill() { killing = true; }

    // Procesa un bloque. in/out pueden ser el mismo buffer. inR/outR pueden ser nullptr.
    void process (const float* inL, const float* inR, float* outL, float* outR, int numSamples)
    {
        const float targetDelay = std::clamp (params.timeMs, kMinTimeMs, kMaxTimeMs) * 0.001f * sr;
        const float glideCoef   = std::exp (-1.0f / (std::max (params.inertiaSec, 0.005f) * sr));
        const float maxSlew     = 0.85f; // el pitch puede ir entre ~0.15x y ~1.85x durante un glide

        const float wf          = std::clamp (params.wowFlutter, 0.0f, 1.0f);
        const float wowDepth    = wf * 0.0030f * sr;   // ~3 ms
        const float flutDepth   = wf * 0.00012f * sr;  // ~0.12 ms
        const float noiseDepth  = wf * 0.0006f * sr;   // deriva aleatoria lenta
        const float wowInc      = 0.55f / sr;
        const float flutInc     = 6.3f / sr;

        const float drive       = std::clamp (params.drive, 0.0f, 1.0f);
        const float satG        = 0.6f + 3.4f * drive * std::sqrt (drive);
        const float asym        = 0.12f * drive;

        const bool  open        = (params.sendOpen || params.throwHeld) && ! params.freeze;
        const float gateTarget  = open ? 1.0f : 0.0f;
        const float fbTarget    = params.freeze ? 1.0f : std::clamp (params.feedback, 0.0f, kMaxFeedback);
        const float filtTarget  = params.freeze ? 0.0f : 1.0f;

        const float killStep    = 1.0f / (0.02f * sr);
        const float dcCoef      = 1.0f - (2.0f * 3.14159265f * 8.0f / sr);

        for (int n = 0; n < numSamples; ++n)
        {
            // --- filtros: recalcular coeficientes cada 16 muestras
            if ((n & 15) == 0)
            {
                hp.setCutoff (lowCutSmooth.process (params.lowCutHz), sr);
                lp.setCutoff (highCutSmooth.process (params.highCutHz), sr);
            }

            // --- tiempo con inercia (glide) limitado en velocidad
            float step = (targetDelay - delaySmp) * (1.0f - glideCoef);
            step = std::clamp (step, -maxSlew, maxSlew);
            delaySmp += step;

            // --- wow & flutter
            wowPhase += wowInc;         if (wowPhase >= 1.0f) wowPhase -= 1.0f;
            flutterPhase += flutInc;    if (flutterPhase >= 1.0f) flutterPhase -= 1.0f;
            if (--noiseCounter <= 0)
            {
                noiseCounter = (int) (sr * 0.25f);
                noiseTarget = nextRandom();
            }
            noiseValue += (noiseTarget - noiseValue) * (4.0f / sr);

            const float mod = wowDepth * (0.5f + 0.5f * std::sin (6.2831853f * wowPhase))
                            + flutDepth * (0.5f + 0.5f * std::sin (6.2831853f * flutterPhase))
                            + noiseDepth * (0.5f + 0.5f * noiseValue);

            const float d = std::max (delaySmp + mod, 3.0f);

            // --- cabeza de reproducción (interpolación Hermite)
            // índice envuelto + double: precisión sub-muestra estable aunque la app corra horas
            const double readPos = (double) (int64_t) (writeIdx & mask) - (double) d;
            const double fl = std::floor (readPos);
            const float t  = (float) (readPos - fl);
            const auto  i  = (int64_t) fl;
            const float ym1 = buffer[(size_t) (i - 1) & mask];
            const float y0  = buffer[(size_t) (i)     & mask];
            const float y1  = buffer[(size_t) (i + 1) & mask];
            const float y2  = buffer[(size_t) (i + 2) & mask];
            const float c1 = 0.5f * (y1 - ym1);
            const float c2 = ym1 - 2.5f * y0 + 2.0f * y1 - 0.5f * y2;
            const float c3 = 0.5f * (y2 - ym1) + 1.5f * (y0 - y1);
            const float played = ((c3 * t + c2) * t + c1) * t + y0;

            // --- filtros del loop (en freeze se puentean para que el loop no se apague)
            float lowOut, highOut, lpOut, dummy;
            hp.process (played, lowOut, highOut);
            lp.process (highOut, lpOut, dummy);
            const float fm = filterMix.process (filtTarget);
            const float colored = fm * lpOut + (1.0f - fm) * played;

            // --- kill
            if (killing)
            {
                killGain -= killStep;
                if (killGain <= 0.0f)
                {
                    std::fill (buffer.begin(), buffer.end(), 0.0f);
                    hp.reset(); lp.reset();
                    killGain = 1.0f;
                    killing = false;
                }
            }
            const float kg = killing ? killGain : 1.0f;

            // --- entrada
            const float xin = inR != nullptr ? 0.5f * (inL[n] + inR[n]) : inL[n];
            const float in  = xin * inSmooth.process (params.inputGain) * gateSmooth.process (gateTarget);

            // --- cabeza de grabación: entrada + realimentación, saturada como cinta
            const float fb  = fbSmooth.process (fbTarget) * kg;
            float rec = in + fb * colored;
            rec = rec + asym * rec * rec;
            rec = std::tanh (satG * rec) / satG;
            if (! std::isfinite (rec)) rec = 0.0f;
            buffer[writeIdx & mask] = rec;
            ++writeIdx;

            // --- salida: solo eco, sin DC, con limitador suave de seguridad
            float y = colored * outSmooth.process (params.outputGain) * kg;
            const float dcOut = y - dcX + dcCoef * dcY;
            dcX = y; dcY = dcOut;
            y = std::tanh (dcOut);

            outL[n] = y;
            if (outR != nullptr) outR[n] = y;
        }
    }

    float getCurrentDelayMs() const { return delaySmp / sr * 1000.0f; }

private:
    float nextRandom()
    {
        rngState = rngState * 1664525u + 1013904223u;
        return (float) (rngState >> 8) / 8388608.0f - 1.0f; // -1..1
    }

    TapeEchoParams params;
    float sr = 48000.0f;

    std::vector<float> buffer;
    size_t mask = 0;
    uint64_t writeIdx = 0;

    float delaySmp = 0.0f;
    float wowPhase = 0.0f, flutterPhase = 0.0f;
    float noiseValue = 0.0f, noiseTarget = 0.0f;
    int   noiseCounter = 0;
    uint32_t rngState = 12345u;

    SVF hp, lp;
    OnePole gateSmooth, fbSmooth, inSmooth, outSmooth, filterMix, lowCutSmooth, highCutSmooth;

    float dcX = 0.0f, dcY = 0.0f;
    float killGain = 1.0f;
    bool  killing = false;
};

} // namespace dubecho
