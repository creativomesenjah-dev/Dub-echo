// SirenDSP.h — sirena de dub: oscilador + LFO de pitch + caída al soltar. Sin dependencias de JUCE.
#pragma once

#include <algorithm>
#include <cmath>

namespace dubecho
{

struct SirenPreset
{
    enum Wave { Square = 0, Triangle, Saw };
    enum LfoShape { LfoSquare = 0, LfoSawDown, LfoTriangle, LfoSine };

    const char* name;
    Wave  wave;
    float baseHz;
    LfoShape lfoShape;
    float lfoHz;
    float lfoSemis;       // cuánto sube el pitch el LFO (en semitonos)
    float holdDriftSemis; // deriva mientras se mantiene (por segundo, negativo = baja)
    float releaseSec;     // duración de la caída al soltar
    float releaseSemis;   // cuánto cae el pitch al soltar
};

// Los cuatro clásicos. Se ajustan de oído desde acá.
inline const SirenPreset& sirenPreset (int i)
{
    static const SirenPreset presets[4] = {
        { "DOS TONOS",  SirenPreset::Square,   740.0f,  SirenPreset::LfoSquare,   2.6f,  5.0f,  0.0f,  0.35f, 12.0f },
        { "LÁSER",      SirenPreset::Square,   880.0f,  SirenPreset::LfoSawDown,  7.0f, 24.0f,  0.0f,  0.25f,  7.0f },
        { "ONDULANTE",  SirenPreset::Triangle, 520.0f,  SirenPreset::LfoTriangle, 0.9f, 12.0f,  0.0f,  0.60f, 12.0f },
        { "CAÍDA",      SirenPreset::Square,  1300.0f,  SirenPreset::LfoSine,     6.0f,  0.6f, -4.0f,  1.40f, 36.0f },
    };
    return presets[std::clamp (i, 0, 3)];
}

class Siren
{
public:
    void prepare (double sampleRate)
    {
        sr = (float) sampleRate;
        reset();
    }

    void reset()
    {
        phase = lfoPhase = 0.0f;
        amp = 0.0f;
        held = false;
        active = false;
        holdTime = releaseTime = 0.0f;
        logFreq = std::log2 (440.0f);
        lpState = 0.0f;
    }

    // Disparar (o cambiar a) un preset. pitchMul / rateMul vuelven a 1.
    void noteOn (int preset)
    {
        const bool wasSilent = ! active;
        current = preset;
        held = true;
        active = true;
        holdTime = 0.0f;
        releaseTime = 0.0f;
        lfoPhase = 0.0f;
        pitchMul = rateMul = 1.0f;
        if (wasSilent)
            logFreq = std::log2 (sirenPreset (preset).baseHz);
    }

    void noteOff() { if (held) { held = false; releaseTime = 0.0f; } }

    // Mientras suena, el pad XY la toca: X = tono (media octava a 2 octavas), Y = velocidad del LFO.
    void setXY (float x01, float y01)
    {
        if (x01 >= 0.0f) pitchMul = std::exp2 ((std::clamp (x01, 0.0f, 1.0f) - 0.5f) * 2.0f);   // ±1 octava
        if (y01 >= 0.0f) rateMul  = std::exp2 ((std::clamp (y01, 0.0f, 1.0f) - 0.5f) * 4.0f);   // x0,25 .. x4
    }

    bool isHeld() const   { return held; }
    bool isActive() const { return active; }
    int  getPreset() const { return current; }

    // Suma la sirena (mono) en out[]; devuelve true si sonó algo.
    bool process (float* out, int n, float level)
    {
        if (! active) return false;
        const auto& p = sirenPreset (current);
        const float dt = 1.0f / sr;
        const float attackStep = dt / 0.004f;
        const float glide = 1.0f - std::exp (-dt / 0.006f);
        const float lpCoef = 1.0f - std::exp (-2.0f * 3.14159265f * 5500.0f * dt);

        for (int i = 0; i < n; ++i)
        {
            // --- envolvente de amplitud y caída de pitch al soltar
            float dropSemis = 0.0f;
            if (held)
            {
                amp = std::min (1.0f, amp + attackStep);
                holdTime += dt;
            }
            else
            {
                releaseTime += dt;
                const float r = releaseTime / p.releaseSec;
                dropSemis = -p.releaseSemis * std::min (r, 1.5f);
                amp *= std::exp (-dt * 4.0f / p.releaseSec);
                if (amp < 0.0005f) { active = false; amp = 0.0f; break; }
            }

            // --- LFO
            lfoPhase += p.lfoHz * rateMul * dt;
            lfoPhase -= std::floor (lfoPhase);
            float lfo;
            switch (p.lfoShape)
            {
                case SirenPreset::LfoSquare:   lfo = lfoPhase < 0.5f ? 0.0f : 1.0f; break;
                case SirenPreset::LfoSawDown:  lfo = 1.0f - lfoPhase; break;
                case SirenPreset::LfoTriangle: lfo = 1.0f - std::abs (2.0f * lfoPhase - 1.0f); break;
                case SirenPreset::LfoSine:
                default:                       lfo = 0.5f + 0.5f * std::sin (6.2831853f * lfoPhase); break;
            }

            const float semis = lfo * p.lfoSemis + p.holdDriftSemis * std::min (holdTime, 4.0f) + dropSemis;
            const float targetLog = std::log2 (p.baseHz * pitchMul) + semis / 12.0f;
            // un mínimo deslizamiento suaviza los saltos del LFO cuadrado (como las sirenas analógicas)
            logFreq += (targetLog - logFreq) * glide;
            const float freq = std::clamp (std::exp2 (logFreq), 20.0f, 0.45f * sr);
            const float inc = freq * dt;

            // --- oscilador con polyBLEP (sin aliasing áspero)
            phase += inc;
            if (phase >= 1.0f) phase -= 1.0f;
            float s;
            switch (p.wave)
            {
                case SirenPreset::Square:
                    s = (phase < 0.5f ? 1.0f : -1.0f) + polyBlep (phase, inc) - polyBlep (std::fmod (phase + 0.5f, 1.0f), inc);
                    break;
                case SirenPreset::Saw:
                    s = 2.0f * phase - 1.0f - polyBlep (phase, inc);
                    break;
                case SirenPreset::Triangle:
                default:
                    s = 1.0f - 4.0f * std::abs (phase - 0.5f);
                    break;
            }

            // --- tono: pasa-bajos suave + un poco de saturación
            lpState += (s - lpState) * lpCoef;
            const float y = std::tanh (1.3f * lpState) * 0.33f * amp * level;
            out[i] += y;
        }
        return true;
    }

private:
    static float polyBlep (float t, float dt)
    {
        if (t < dt)        { t /= dt; return t + t - t * t - 1.0f; }
        if (t > 1.0f - dt) { t = (t - 1.0f) / dt; return t * t + t + t + 1.0f; }
        return 0.0f;
    }

    float sr = 48000.0f;
    float phase = 0.0f, lfoPhase = 0.0f, amp = 0.0f;
    float holdTime = 0.0f, releaseTime = 0.0f;
    float logFreq = 8.78f, lpState = 0.0f;
    float pitchMul = 1.0f, rateMul = 1.0f;
    int current = 0;
    bool held = false, active = false;
};

} // namespace dubecho
