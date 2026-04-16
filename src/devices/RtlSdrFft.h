#pragma once

#include <QVector>
#include <kiss_fft.h>

class RtlSdrFft
{
public:
    explicit RtlSdrFft(int fftSize);
    ~RtlSdrFft();

    RtlSdrFft(const RtlSdrFft&) = delete;
    RtlSdrFft& operator=(const RtlSdrFft&) = delete;

    int fftSize() const { return m_fftSize; }

    void setCalibrationOffset(double dB) { m_calibrationOffsetDb = dB; }
    double calibrationOffset() const { return m_calibrationOffsetDb; }

    // Build the 256-entry decompanding lookup table (µ-law-style expansion)
    static void buildDecompandingLut(float lut[256]);

    // Compute power spectrum from interleaved 8-bit I/Q samples.
    // Applies decompanding, Hann window, FFT, magnitude², dBm conversion, and FFT-shift.
    // Input:  raw unsigned 8-bit I/Q pairs (size = fftSize * 2 bytes)
    // Output: outDbm resized to fftSize, frequency-ascending dBm values
    void computePowerSpectrum(const unsigned char* iqSamples,
                             const float decompandLut[256],
                             QVector<double>& outDbm) const;

private:
    int m_fftSize;
    double m_calibrationOffsetDb = -20.0; // Empirical offset for RTL-SDR
    kiss_fft_cfg m_fftCfg = nullptr;
    QVector<float> m_window; // Precomputed Hann window coefficients
};
