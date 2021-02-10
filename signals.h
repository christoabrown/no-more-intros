#ifndef SIGNALS_H
#define SIGNALS_H

// comment this line to deactivate OpenMP for loop parallelizations, or if you want to debug
// memory management (valgrind reports OMP normal activity as error).
// the number is the minimum size that a 'for' loop needs to get sent to OMP (1=>always sent)
#define WITH_OPENMP_ABOVE 1
#define REAL 0
#define IMAG 1

#include <iostream>
#include <sstream>
#include <stdexcept>
#include <vector>
#include <fftw3.h>
#ifdef WITH_OPENMP_ABOVE
#include <omp.h>
#endif

size_t Pow2Ceil(size_t x);

// Given a container or its beginning and end iterables, checks wether all values contained in the
// iterable are equal and raises an exception if not. Usage example:
// vector<size_t> v1({});
// vector<double> v2({123.4, 123.4, 123.4});
// vector<bool> v3({false, false, false});
// vector<size_t> v4({1});
// vector<string> v5({"hello", "hello", "bye"});
// CheckAllEqual({3,3,3,3,3,3,3,3});
// CheckAllEqual(v1);
// CheckAllEqual(v2.begin(), v2.end());
// CheckAllEqual(v3);
// CheckAllEqual(v4);
// CheckAllEqual(v5.begin(), prev(v5.end()));
// CheckAllEqual(v5);
template<class I>
void CheckAllEqual(const I beg, const I end, const std::string& message = "CheckAllEqual") {
    I it = beg;
    bool all_eq = true;
    auto last = (it == end) ? end : std::prev(end);
    for (; it != last; ++it) {
        all_eq &= (*(it) == *(std::next(it)));
        if (!all_eq) {
            throw std::runtime_error(std::string("[ERROR] ") + message);
        }
    }
}
template <class C>
void CheckAllEqual(const C& c, const std::string message = "CheckAllEqual") {
    CheckAllEqual(c.begin(), c.end(), message);
}
template <class T>
void CheckAllEqual(const std::initializer_list<T> c, const std::string message = "CheckAllEqual") {
    CheckAllEqual(c.begin(), c.end(), message);
}

// Raises an exception if complex_size!=(real_size/2+1), being "/" an integer division.
void CheckRealComplexRatio(const size_t real_size, const size_t complex_size,
    const std::string func_name = "CheckRealComplexRatio");

// Abstract function that performs a comparation between any 2 elements, and if the comparation
// returns a truthy value raises an exception with the given message.
template <class T, class Functor>
void CheckTwoElements(const T a, const T b, const Functor& binary_predicate,
    const std::string message) {
    if (binary_predicate(a, b)) {
        throw std::runtime_error(std::string("[ERROR] ") + message);
    }
}

// Raises an exception with the given message if a>b.
void check_a_less_equal_b(const size_t a, const size_t b,
    const std::string message = "a was greater than b!");

 // This is an abstract base class that provides some basic, type-independent functionality for
 // any container that should behave as a signal. It is not intended to be instantiated directly.
template <class T>
class Signal {
protected:
    T* data_;
    size_t size_;
public:
    // Given a size and a reference to an array, it fills the array with <SIZE> zeros.
    // Therefore, **IT DELETES THE CONTENTS OF THE ARRAY**. It is intended to be passed a newly
    // allocated array by the classes that inherit from Signal, because it isn't an expensive
    // operation and avoids memory errors due to non-initialized values.
    explicit Signal(T* data, size_t size) : data_(data), size_(size) {
        memset(data_, 0, sizeof(T) * size);
    }
    // The destructor is empty because this class didn't allocate the contained array
    virtual ~Signal() {}
    // getters
    size_t& getSize() { return size_; }
    const size_t& getSize() const { return size_; }
    T* getData() { return data_; }
    const T* getData() const { return data_; }
    // overloaded operators
    T& operator[](size_t idx) { return data_[idx]; }
    T& operator[](size_t idx) const { return data_[idx]; }
    // basic print function. It may be overriden if, for example, the type <T> is a struct.
    void print(const std::string name = "signal") {
        std::cout << std::endl;
        for (size_t i = 0; i < size_; ++i) {
            std::cout << name << "[" << i << "]\t=\t" << data_[i] << std::endl;
        }
    }
};

// This class is a Signal that works on aligned float arrays allocated by FFTW.
// It also overloads some further operators to do basic arithmetic
class FloatSignal : public Signal<float> {
public:
    // the basic constructor allocates an aligned, float array, which is zeroed by the superclass
    explicit FloatSignal(size_t size)
        : Signal(fftwf_alloc_real(size), size) {}
    explicit FloatSignal(float* data, size_t size) : FloatSignal(size) {
        memcpy(data_, data, sizeof(float) * size);
    }
    explicit FloatSignal(float* data, size_t size, size_t pad_bef, size_t pad_aft)
        : FloatSignal(size + pad_bef + pad_aft) {
        memcpy(data_ + pad_bef, data, sizeof(float) * size);
    }
    // the destructor frees the only resource allocated
    ~FloatSignal() { fftwf_free(data_); }
    void operator+=(const float x) { for (size_t i = 0; i < size_; ++i) { data_[i] += x; } }
    void operator-=(const float x) { for (size_t i = 0; i < size_; ++i) { data_[i] -= x; } }
    void operator*=(const float x) { for (size_t i = 0; i < size_; ++i) { data_[i] *= x; } }
    void operator/=(const float x) { for (size_t i = 0; i < size_; ++i) { data_[i] /= x; } }
    float mean() const {
        float value_sum = 0.0f;
        size_t size = size_;
        float* data = data_;

        for (size_t i = 0; i < size; ++i) {
            value_sum += data[i];
        }

        const float result = value_sum / size;
        return result;
    }

    float std() const {
        const float mean = this->mean();
        size_t size = size_;
        float* data = data_;
        float result = 0;

        for (size_t i = 0; i < size; ++i) {
            float a = fabs(data[i] - mean);
            result += a * a;
        }
        result = sqrtf(result / size);
        return result;
    }
};

// This class is a Signal that works on aligned complex (float[2]) arrays allocated by FFTW.
// It also overloads some further operators to do basic arithmetic
class ComplexSignal : public Signal<fftwf_complex> {
public:
    // the basic constructor allocates an aligned, float[2] array, which is zeroed by the superclass
    explicit ComplexSignal(size_t size)
        : Signal(fftwf_alloc_complex(size), size) {}
    ~ComplexSignal() { fftwf_free(data_); }
    void operator*=(const float x) {
        for (size_t i = 0; i < size_; ++i) {
            data_[i][REAL] *= x;
            data_[i][IMAG] *= x;
        }
    }
    void operator+=(const float x) { for (size_t i = 0; i < size_; ++i) { data_[i][REAL] += x; } }
    void operator+=(const fftwf_complex x) {
        for (size_t i = 0; i < size_; ++i) {
            data_[i][REAL] += x[REAL];
            data_[i][IMAG] += x[IMAG];
        }
    }
    // override print method to show both fields of the complex number
    void print(const std::string name = "signal") {
        for (size_t i = 0; i < size_; ++i) {
            printf("%s[%zu]\t=\t(%f, i%f)\n", name.c_str(), i, data_[i][REAL], data_[i][IMAG]);
        }
    }
};

// This class is a simple wrapper for the memory management of the fftw plans, plus a
// parameterless execute() method which is also a wrapper for FFTW's execute.
// It is not expected to be used directly: rather, to be extended by specific plans, for instance,
// if working with real, 1D signals, only 1D complex<->real plans are needed.
class FftPlan {
private:
    fftwf_plan plan_;
public:
    explicit FftPlan(fftwf_plan p) : plan_(p) {}
    virtual ~FftPlan() { fftwf_destroy_plan(plan_); }
    void execute() { fftwf_execute(plan_); }
};

// This forward plan (1D, R->C) is adequate to process 1D floats (real).
class FftForwardPlan : public FftPlan {
public:
    // This constructor creates a real->complex plan that performs the FFT(real) and saves it into the
    // complex. As explained in the FFTW docs (http://www.fftw.org/#documentation), the size of
    // the complex has to be size(real)/2+1, so the constructor will throw a runtime error if
    // this condition doesn't hold. Since the signals and the superclass already have proper
    // destructors, no special memory management has to be done.
    explicit FftForwardPlan(FloatSignal& fs, ComplexSignal& cs)
        : FftPlan(fftwf_plan_dft_r2c_1d((int)fs.getSize(), fs.getData(), cs.getData(), FFTW_ESTIMATE)) {
        CheckRealComplexRatio(fs.getSize(), cs.getSize(), "FftForwardPlan");
    }
};

// This backward plan (1D, C->R) is adequate to process spectra of 1D floats (real).
class FftBackwardPlan : public FftPlan {
public:
    // This constructor creates a complex->real plan that performs the IFFT(complex) and saves it
    // complex. As explained in the FFTW docs (http://www.fftw.org/#documentation), the size of
    // the complex has to be size(real)/2+1, so the constructor will throw a runtime error if
    // this condition doesn't hold. Since the signals and the superclass already have proper
    // destructors, no special memory management has to be done.
    explicit FftBackwardPlan(ComplexSignal& cs, FloatSignal& fs)
        : FftPlan(fftwf_plan_dft_c2r_1d((int)fs.getSize(), cs.getData(), fs.getData(), FFTW_ESTIMATE)) {
        CheckRealComplexRatio(fs.getSize(), cs.getSize(), "FftBackwardPlan");
    }
};

// This free function takes three complex signals a,b,c of the same size and computes the complex
// element-wise multiplication:   a+ib * c+id = ac+iad+ibc-bd = ac-bd + i(ad+bc)   The computation
// loop isn't sent to OMP because this function itself is already expected to be called by multiple
// threads, and it would actually slow down the process.
// It throuws an exception if
void SpectralConvolution(const ComplexSignal& a, const ComplexSignal& b, ComplexSignal& result);

// This function behaves identically to SpectralConvolution, but computes c=a*conj(b) instead
// of c=a*b:         a * conj(b) = a+ib * c-id = ac-iad+ibc+bd = ac+bd + i(bc-ad)
void SpectralCorrelation(const ComplexSignal& a, const ComplexSignal& b, ComplexSignal& result);

// This function is a small script that calculates the FFT wisdom for all powers of two (since those
// are the only expected sizes to be used with the FFTs), and exports it to the given path. The
// wisdom is a brute-force search of the most efficient implementations for the FFTs: It takes a
// while to compute, but has to be done only once (per computer), and then it can be quickly loaded
// for faster FFT computation, as explained in the docs (http://www.fftw.org/#documentation).
// See also the docs for different flags. Note that using a wisdom file is optional.
void MakeAndExportFftwWisdom(const std::string path_out, const size_t min_2pow = 0,
    const size_t max_2pow = 25, const unsigned flag = FFTW_PATIENT);

// Given a path to a wisdom file generated with "MakeAndExportFftwWisdom", reads and loads it
// into FFTW to perform faster FFT computations. Using a wisdom file is optional.
void ImportFftwWisdom(const std::string path_in, const bool throw_exception_if_fail = true);

////////////////////////////////////////////////////////////////////////////////////////////////////
/// PERFORM CONVOLUTION/CORRELATION
////////////////////////////////////////////////////////////////////////////////////////////////////

// This class performs an efficient version of the spectral convolution/cross-correlation between
// two 1D float arrays, <SIGNAL> and <PATCH>, called overlap-save:
// http://www.comm.utoronto.ca/~dkundur/course_info/real-time-DSP/notes/8_Kundur_Overlap_Save_Add.pdf
// This algorithm requires that the length of <PATCH> is less or equal the length of <SIGNAL>,
// so an exception is thrown otherwise. The algorithm works as follows:
// given signal of length S and patch of length P, and being the conv (or xcorr) length U=S+P-1
//   1. pad the patch to X = 2*Pow2Ceil(P). FFTs with powers of 2 are the fastest.
//   2. cut the signal into chunks of size X, with an overlapping section of L=X-(P-1).
//      for that, pad the signal with (P-1) before, and with (X-U%L) after, to make it fit exactly.
//   3. Compute the forward FFT of the padded patch and of every chunk of the signal
//   4. Multiply the FFT of the padded patch with every signal chunk.
//      4a. If the operation is a convolution, perform a complex a*b multiplication
//      4b. If the operation is a cross-correlation, perform a complex a*conj(b) multiplication
//   5. Compute the inverse FFT of every result of step 4
//   6. Concatenate the resulting chunks, ignoring (P-1) samples per chunk
// Note that steps 3,4,5 may be parallelized with some significant gain in performance.
// In this class: X = result_chunksize, L = result_stride
class OverlapSaveConvolver {
private:
    // grab input lengths
    size_t signal_size_;
    size_t patch_size_;
    size_t result_size_;
    // make padded copies of the inputs and get chunk measurements
    FloatSignal padded_patch_;
    size_t result_chunksize_;
    size_t result_chunksize_complex_;
    size_t result_stride_;
    ComplexSignal padded_patch_complex_;
    // padded copy of the signal
    FloatSignal padded_signal_;
    // the deconstructed signal
    std::vector<FloatSignal*> s_chunks_;
    std::vector<ComplexSignal*> s_chunks_complex_;
    // the corresponding chunks holding convs/xcorrs
    std::vector<FloatSignal*> result_chunks_;
    std::vector<ComplexSignal*> result_chunks_complex_;
    // the corresponding plans (plus the plan of the patch)
    std::vector<FftForwardPlan*> forward_plans_;
    std::vector<FftBackwardPlan*> backward_plans_;

    // Basic state management to prevent getters from being called prematurely.
    // Also to adapt the extractResult getter, since Conv and Xcorr padding behaves differently
    enum class State { kUninitialized, kConv, kXcorr };
    State _state_; // kUninitialized after instantiation, kConv/kXcorr after respective op.
    // This private method throws an exception if _state_ is kUninitialized, because that
    // means that some "getter" has ben called before any computation has been performed.
    void __check_last_executed_not_null(const std::string method_name) {
        if (_state_ == State::kUninitialized) {
            throw std::runtime_error(std::string("[ERROR] OverlapSaveConvolver.") + method_name +
                "() can't be called before executeXcorr() or executeConv()!" +
                " No meaningful data has been computed yet.");
        }
    }

    // This private method implements steps 3,4,5 of the algorithm. If the given flag is false,
    // it will perform a convolution (4a), and a cross-correlation (4b) otherwise.
    // Note the parallelization with OpenMP, which increases performance in supporting CPUs.
    void __execute(const bool cross_correlate) {
        auto operation = (cross_correlate) ? SpectralCorrelation : SpectralConvolution;

        // do ffts
#ifdef WITH_OPENMP_ABOVE
#pragma omp parallel for schedule(static, WITH_OPENMP_ABOVE)
#endif
        for (long long i = 0; i < (long long)forward_plans_.size(); i++) {
            forward_plans_.at(i)->execute();
        }
        // multiply spectra
#ifdef WITH_OPENMP_ABOVE
#pragma omp parallel for schedule(static, WITH_OPENMP_ABOVE)
#endif
        for (long long i = 0; i < (long long)result_chunks_.size(); i++) {
            operation(*s_chunks_complex_.at(i), this->padded_patch_complex_, *result_chunks_complex_.at(i));
        }
        // do iffts
#ifdef WITH_OPENMP_ABOVE
#pragma omp parallel for schedule(static, WITH_OPENMP_ABOVE)
#endif
        for (long long i = 0; i < (long long)result_chunks_.size(); i++) {
            backward_plans_.at(i)->execute();
            *result_chunks_.at(i) /= (float)result_chunksize_;
        }
    }

public:
    // The only constructor for the class, receives two signals and performs steps 1 and 2 of the
    // algorithm on them. The signals are passed by reference but the class works with padded copies
    // of them, so no care has to be taken regarding memory management.
    // The wisdomPath may be empty, or a path to a valid wisdom file.
    // Note that len(signal) can never be smaller than len(patch), or an exception is thrown.
    OverlapSaveConvolver(FloatSignal& signal, FloatSignal& patch, const std::string wisdomPath = "")
        : signal_size_(signal.getSize()),
        patch_size_(patch.getSize()),
        result_size_(signal_size_ + patch_size_ - 1),
        //
        padded_patch_(patch.getData(), patch_size_, 0, 2 * Pow2Ceil(patch_size_) - patch_size_),
        result_chunksize_(padded_patch_.getSize()),
        result_chunksize_complex_(result_chunksize_ / 2 + 1),
        result_stride_(result_chunksize_ - patch_size_ + 1),
        padded_patch_complex_(result_chunksize_complex_),
        //
        padded_signal_(signal.getData(), signal_size_, patch_size_ - 1, result_chunksize_ - (result_size_ % result_stride_)),
        _state_(State::kUninitialized) {
        // end of initializer list, now check that len(signal)>=len(patch)
        check_a_less_equal_b(patch_size_, signal_size_,
            "OverlapSaveConvolver: len(signal) can't be smaller than len(patch)!");
        // and load the wisdom if required. If unsuccessful, no exception thrown, just print a warning.
        if (!wisdomPath.empty()) { ImportFftwWisdom(wisdomPath, false); }
        // chunk the signal into strides of same size as padded patch
        // and make complex counterparts too, as well as the corresponding xcorr signals
        for (size_t i = 0; i <= padded_signal_.getSize() - result_chunksize_; i += result_stride_) {
            s_chunks_.push_back(new FloatSignal(&padded_signal_[i], result_chunksize_));
            s_chunks_complex_.push_back(new ComplexSignal(result_chunksize_complex_));
            result_chunks_.push_back(new FloatSignal(result_chunksize_));
            result_chunks_complex_.push_back(new ComplexSignal(result_chunksize_complex_));
        }
        // make one forward plan per signal chunk, and one for the patch
        // Also backward plans for the xcorr chunks
        forward_plans_.push_back(new FftForwardPlan(padded_patch_, padded_patch_complex_));
        for (size_t i = 0; i < s_chunks_.size(); i++) {
            forward_plans_.push_back(new FftForwardPlan(*s_chunks_.at(i), *s_chunks_complex_.at(i)));
            backward_plans_.push_back(new FftBackwardPlan(*result_chunks_complex_.at(i),
                *result_chunks_.at(i)));
        }
    }

    //
    void executeConv() {
        __execute(false);
        _state_ = State::kConv;
    }
    void executeXcorr() {
        __execute(true);
        _state_ = State::kXcorr;
    }
    // getting info from the convolfer
    void printChunks(const std::string name = "convolver") {
        __check_last_executed_not_null("printChunks");
        for (size_t i = 0; i < result_chunks_.size(); i++) {
            result_chunks_.at(i)->print(name + "_chunk_" + std::to_string(i));
        }
    }

    // This method implements step 6 of the overlap-save algorithm. In convolution, the first (P-1)
    // samples of each chunk are discarded, in xcorr the last (P-1) ones. Therefore, depending on the
    // current _state_, the corresponding method is used. USAGE:
    // Every time it is called, this function returns a new FloatSignal instance of size
    // len(signal)+len(patch)-1. If the last operation performed was executeConv(), this function
    // will return the  convolution of signal and patch. If the last operation performed was
    // executeXcorr(), the result will contain the cross-correlation. If none of them was performed
    // at the moment of calling this function, an exception will be thrown.
    // The indexing will start with the most negative relation, and increase accordingly. Which means:
    //   given S:=len(signal), P:=len(patch), T:=S+P-1
    // for 0 <= i < T, result[i] will hold dot_product(patch, signal[i-(P-1) : i])
    //   where patch will be "reversed" if the convolution was performed. For example:
    // Signal :=        [1 2 3 4 5 6 7]    Patch = [1 1 1]
    // Result[0] =  [1 1 1]                        => 1*1         = 1  // FIRST ENTRY
    // Result[1] =    [1 1 1]                      => 1*1+1*2     = 3
    // Result[2] =      [1 1 1]                    => 1*1+1*2+1*3 = 8  // FIRST NON-NEG ENTRY AT P-1
    //   ...
    // Result[8] =                  [1 1 1]        => 1*7         = 7  // LAST ENTRY
    // Note that the returned signal object takes care of its own memory, so no management is needed.
    FloatSignal* extractResult() {
        // make sure that an operation was called before
        __check_last_executed_not_null("extractResult");
        // set the offset for the corresponding operation (0 for xcorr).
        size_t discard_offset = 0;
        if (_state_ == State::kConv) { discard_offset = result_chunksize_ - result_stride_; }
        // instantiate new signal to be filled with the desired info
        FloatSignal* result = new FloatSignal(result_size_);
        float* result_arr = result->getData(); // not const because of memcpy
        // fill!
        size_t kNumChunks = result_chunks_.size();
        for (size_t i = 0; i < kNumChunks; i++) {
            float* xc_arr = result_chunks_.at(i)->getData();
            const size_t kBegin = i * result_stride_;
            // if the last chunk goes above result_size_, reduce copy size. else copy_size=result_stride_
            size_t copy_size = result_stride_;
            copy_size -= (kBegin + result_stride_ > result_size_) ? kBegin + result_stride_ - result_size_ : 0;
            memcpy(result_arr + kBegin, xc_arr + discard_offset, sizeof(float) * copy_size);
        }
        return result;
    }

    ~OverlapSaveConvolver() {
        // clear vectors holding signals
        for (size_t i = 0; i < s_chunks_.size(); i++) {
            delete (s_chunks_.at(i));
            delete (s_chunks_complex_.at(i));
            delete (result_chunks_.at(i));
            delete (result_chunks_complex_.at(i));
        }
        s_chunks_.clear();
        s_chunks_complex_.clear();
        result_chunks_.clear();
        result_chunks_complex_.clear();
        // clear vector holding forward FFT plans
        for (size_t i = 0; i < forward_plans_.size(); i++) {
            delete (forward_plans_.at(i));
        }
        forward_plans_.clear();
        // clear vector holding backward FFT plans
        for (size_t i = 0; i < backward_plans_.size(); i++) {
            delete (backward_plans_.at(i));
        }
        backward_plans_.clear();
    }
};

#endif // SIGNALS_H
