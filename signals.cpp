#include "signals.h"

size_t Pow2Ceil(size_t x) { return (size_t)pow(2, ceil(log2(x))); }

void CheckRealComplexRatio(const size_t real_size, const size_t complex_size,
   const std::string func_name) {
    if (complex_size != (real_size / 2 + 1)) {
        throw std::runtime_error(std::string("[ERROR] ") + func_name +
            ": size of ComplexSignal must equal size(FloatSignal)/2+1. ");
    }
}

void check_a_less_equal_b(const size_t a, const size_t b,
    const std::string message) {
    CheckTwoElements(a, b, [](const size_t a, const size_t b) {return a > b; }, message);
}

void SpectralConvolution(const ComplexSignal& a, const ComplexSignal& b, ComplexSignal& result) {
    const size_t kSize_a = a.getSize();
    const size_t kSize_b = b.getSize();
    const size_t kSize_result = result.getSize();
    CheckAllEqual({ kSize_a, kSize_b, kSize_result },
        "SpectralConvolution: all sizes must be equal and are");
    for (size_t i = 0; i < kSize_a; ++i) {
        // a+ib * c+id = ac+iad+ibc-bd = ac-bd + i(ad+bc)
        result[i][REAL] = a[i][REAL] * b[i][REAL] - a[i][IMAG] * b[i][IMAG];
        result[i][IMAG] = a[i][IMAG] * b[i][REAL] + a[i][REAL] * b[i][IMAG];
    }
}

void SpectralCorrelation(const ComplexSignal& a, const ComplexSignal& b, ComplexSignal& result) {
    const size_t kSize_a = a.getSize();
    const size_t kSize_b = b.getSize();
    const size_t kSize_result = result.getSize();
    CheckAllEqual({ kSize_a, kSize_b, kSize_result },
        "SpectralCorrelation: all sizes must be equal and are");
    for (size_t i = 0; i < kSize_a; ++i) {
        // a * conj(b) = a+ib * c-id = ac-iad+ibc+bd = ac+bd + i(bc-ad)
        result[i][REAL] = a[i][REAL] * b[i][REAL] + a[i][IMAG] * b[i][IMAG];
        result[i][IMAG] = a[i][IMAG] * b[i][REAL] - a[i][REAL] * b[i][IMAG];

    }
}

void MakeAndExportFftwWisdom(const std::string path_out, const size_t min_2pow,
    const size_t max_2pow, const unsigned flag) {
    for (size_t i = min_2pow; i <= max_2pow; ++i) {
        size_t size = (size_t)pow(2, i);
        FloatSignal fs(size);
        ComplexSignal cs(size / 2 + 1);
        printf("creating forward and backward plans for size=2**%zu=%zu and flag %u...\n", i, size, flag);
        FftForwardPlan fwd(fs, cs);
        FftBackwardPlan bwd(cs, fs);
    }
    fftwf_export_wisdom_to_filename(path_out.c_str());
}

void ImportFftwWisdom(const std::string path_in, const bool throw_exception_if_fail) {
    int result = fftwf_import_wisdom_from_filename(path_in.c_str());
    if (result != 0) {
        std::cout << "[ImportFftwWisdom] succesfully imported " << path_in << std::endl;
    }
    else {
        std::string message = "[ImportFftwWisdom] ";
        message += "couldn't import wisdom! is this a path to a valid wisdom file? -->" + path_in + "<--\n";
        if (throw_exception_if_fail) { throw std::runtime_error(std::string("ERROR: ") + message); }
        else { std::cout << "WARNING: " << message; }
    }
}
