#include "RtlSdrFft.h"

#include <cmath>
#include <cstring>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

RtlSdrFft::RtlSdrFft(int fftSize)
    : m_fftSize(fftSize)
{
    m_fftCfg = kiss_fft_alloc(fftSize, 0 /* forward */, nullptr, nullptr);

    // Precompute Hann window
    m_window.resize(fftSize);
    for (int i = 0; i < fftSize; ++i) {
        m_window[i] = 0.5f * (1.0f - std::cos(2.0f * static_cast<float>(M_PI) * i / (fftSize - 1)));
    }
}

RtlSdrFft::~RtlSdrFft()
{
    if (m_fftCfg) {
        kiss_fft_free(m_fftCfg);
        m_fftCfg = nullptr;
    }
}

void RtlSdrFft::buildDecompandingLut(float lut[256])
{
    // µ-law-style decompanding for RTL2832U 8-bit ADC.
    // Maps unsigned 8-bit samples (0-255) to expanded float values.
    // Center point is 127.5 (DC offset of unsigned 8-bit).
    for (int i = 0; i < 256; ++i) {
        float x = (static_cast<float>(i) - 127.5f) / 127.5f; // Normalize to [-1, +1]
        float sign = (x >= 0.0f) ? 1.0f : -1.0f;
        float absX = std::fabs(x);
        // Expand compressed dynamic range: pow curve recovers ~3-5 dB
        lut[i] = sign * (std::pow(256.0f, absX) - 1.0f) / 255.0f;
    }
}

void RtlSdrFft::computePowerSpectrum(const unsigned char* iqSamples,
                                     const float decompandLut[256],
                                     QVector<double>& outDbm) const
{
    const int N = m_fftSize;
    outDbm.resize(N);

    // Allocate input/output buffers on stack for small sizes, heap for large
    QVector<kiss_fft_cpx> fftIn(N);
    QVector<kiss_fft_cpx> fftOut(N);

    // Decompand + window: convert interleaved uint8 I/Q → complex float
    for (int i = 0; i < N; ++i) {
        float I = decompandLut[iqSamples[i * 2]];
        float Q = decompandLut[iqSamples[i * 2 + 1]];
        fftIn[i].r = I * m_window[i];
        fftIn[i].i = Q * m_window[i];
    }

    // Forward FFT
    kiss_fft(m_fftCfg, fftIn.data(), fftOut.data());

    // Magnitude² → dBm with FFT-shift (put DC in center, then reorder for
    // frequency-ascending output: negative freqs first, then positive)
    //
    // FFT output bins:  [0, 1, ..., N/2-1, N/2, N/2+1, ..., N-1]
    //                    DC  pos freqs     Nyquist  neg freqs
    // We want ascending: [N/2, N/2+1, ..., N-1, 0, 1, ..., N/2-1]
    //                     neg freqs             DC  pos freqs
    const int half = N / 2;
    const double normFactor = static_cast<double>(N) * static_cast<double>(N);

    for (int i = 0; i < N; ++i) {
        int srcBin = (i + half) % N; // FFT-shift index
        double re = fftOut[srcBin].r;
        double im = fftOut[srcBin].i;
        double magSq = (re * re + im * im) / normFactor;
        // Convert to dBm (dB relative to milliwatt, using 50 ohm reference)
        // 10*log10(magSq) + calibration offset
        if (magSq < 1e-20)
            magSq = 1e-20; // Floor to prevent -inf
        outDbm[i] = 10.0 * std::log10(magSq) + m_calibrationOffsetDb;
    }
}
