#pragma once

#include <juce_core/juce_core.h>
#include <array>
#include <cmath>
#include <complex>
#include <vector>

/*
    Shared building blocks for the coloured EQs:
      - RBJ-style biquad designs (double precision) and response evaluation
      - the analog character path: input transformer ("iron") and tube make-up stage ("drive")
*/
namespace analogeq
{
    struct Coefs { double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0; };

    struct Biquad
    {
        Coefs c;
        double z1 = 0, z2 = 0;
        double process (double x) noexcept
        {
            const double y = c.b0 * x + z1;
            z1 = c.b1 * x - c.a1 * y + z2;
            z2 = c.b2 * x - c.a2 * y;
            return y;
        }
        void reset() noexcept { z1 = z2 = 0; }
    };

    inline Coefs normalise (double b0, double b1, double b2, double a0, double a1, double a2)
    {
        return { b0 / a0, b1 / a0, b2 / a0, a1 / a0, a2 / a0 };
    }

    inline double clampFreq (double f, double fs) { return juce::jlimit (5.0, 0.45 * fs, f); }

    inline Coefs peak (double fs, double f, double q, double db)
    {
        const double A = std::pow (10.0, db / 40.0), w = juce::MathConstants<double>::twoPi * clampFreq (f, fs) / fs;
        const double c = std::cos (w), al = std::sin (w) / (2.0 * q);
        return normalise (1 + al * A, -2 * c, 1 - al * A, 1 + al / A, -2 * c, 1 - al / A);
    }

    inline Coefs lowShelf (double fs, double f, double q, double db)
    {
        const double A = std::pow (10.0, db / 40.0), w = juce::MathConstants<double>::twoPi * clampFreq (f, fs) / fs;
        const double c = std::cos (w), al = std::sin (w) / (2.0 * q), sa = 2.0 * std::sqrt (A) * al;
        return normalise (A * ((A + 1) - (A - 1) * c + sa), 2 * A * ((A - 1) - (A + 1) * c), A * ((A + 1) - (A - 1) * c - sa),
                          (A + 1) + (A - 1) * c + sa, -2 * ((A - 1) + (A + 1) * c), (A + 1) + (A - 1) * c - sa);
    }

    inline Coefs highShelf (double fs, double f, double q, double db)
    {
        const double A = std::pow (10.0, db / 40.0), w = juce::MathConstants<double>::twoPi * clampFreq (f, fs) / fs;
        const double c = std::cos (w), al = std::sin (w) / (2.0 * q), sa = 2.0 * std::sqrt (A) * al;
        return normalise (A * ((A + 1) + (A - 1) * c + sa), -2 * A * ((A - 1) + (A + 1) * c), A * ((A + 1) + (A - 1) * c - sa),
                          (A + 1) - (A - 1) * c + sa, 2 * ((A - 1) - (A + 1) * c), (A + 1) - (A - 1) * c - sa);
    }

    inline Coefs highPass (double fs, double f, double q)
    {
        const double w = juce::MathConstants<double>::twoPi * clampFreq (f, fs) / fs, c = std::cos (w), al = std::sin (w) / (2.0 * q);
        return normalise ((1 + c) / 2, -(1 + c), (1 + c) / 2, 1 + al, -2 * c, 1 - al);
    }

    inline Coefs lowPass (double fs, double f, double q)
    {
        const double w = juce::MathConstants<double>::twoPi * clampFreq (f, fs) / fs, c = std::cos (w), al = std::sin (w) / (2.0 * q);
        return normalise ((1 - c) / 2, 1 - c, (1 - c) / 2, 1 + al, -2 * c, 1 - al);
    }

    inline std::complex<double> response (const Coefs& c, double f, double fs)
    {
        const std::complex<double> z = std::polar (1.0, -juce::MathConstants<double>::twoPi * f / fs);
        return (c.b0 + c.b1 * z + c.b2 * z * z) / (1.0 + c.a1 * z + c.a2 * z * z);
    }

    inline double magnitudeDb (const std::vector<Coefs>& cs, double f, double fs)
    {
        std::complex<double> h = 1.0;
        for (const auto& c : cs)
            h *= response (c, f, fs);
        return 20.0 * std::log10 (std::abs (h) + 1.0e-12);
    }

    /** Transformer + tube, per channel. Run it at an oversampled rate. */
    struct Character
    {
        void prepare (double rate)
        {
            fs = rate;
            lpCoef = 1.0 - std::exp (-juce::MathConstants<double>::twoPi * 120.0 / fs);
            dcCoef = 1.0 - juce::MathConstants<double>::twoPi * 5.0 / fs;
            reset();
        }

        void reset() { ironLp = ironHf = dcX = dcY = 0.0; }

        /** Input transformer: low band saturates first (core), top end rounds off with more iron. */
        double iron (double x, double amount) noexcept
        {
            const double k = 0.3 + 5.0 * amount * amount;
            ironLp += (x - ironLp) * lpCoef;
            x = (x - ironLp) + std::tanh (k * ironLp) / k;
            const double hf = 1.0 - std::exp (-juce::MathConstants<double>::twoPi * (40000.0 - 22000.0 * amount) / fs);
            ironHf += (x - ironHf) * hf;
            return ironHf;
        }

        /** Tube make-up stage: unity small-signal gain, asymmetric (even harmonics) and growing with drive. */
        double tube (double x, double drive, double& level) noexcept
        {
            const double g = std::pow (10.0, 0.7 * drive);
            const double bias = 0.22 + 0.5 * drive;
            const double tb = std::tanh (bias), slope = 1.0 - tb * tb;
            const double u = g * x;
            level = std::abs (u);
            const double y = (std::tanh (u + bias) - tb) / (slope * g);
            const double out = y - dcX + dcCoef * dcY;
            dcX = y;
            dcY = out;
            return out;
        }

        double fs = 192000.0, lpCoef = 0.0, dcCoef = 0.999;
        double ironLp = 0.0, ironHf = 0.0, dcX = 0.0, dcY = 0.0;
    };
}
