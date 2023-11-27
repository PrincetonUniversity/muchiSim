#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <iostream>
#include <assert.h>
using namespace std;

#define TWO_PI (6.2831853071795864769252867665590057683943L)

// Utility function to swap float values
void swap(float* a, float* b) {
    float temp = *a;
    *a = *b;
    *b = temp;
}

void bit_reversal(float *array, int N) {
    int i, j = 0;
    int N2 = N * 2;
    for (i = 0; i < N2; i += 2) {
        if (j > i) {
            swap(array[j], array[i]);
            swap(array[j+1], array[i+1]);
        }
        int m = N;
        while (m >= 2 && j >= m) {
            j -= m;
            m /= 2;
        }
        j += m;
    }
}

void fft(float* array, int N) {
    int N2 = N * 2;  // Total number of float elements, considering real and imaginary parts
    bit_reversal(array, N);

    for (int m = 2; m <= N; m *= 2) {
        int mh = m / 2;
        for (int i = 0; i < N; i += m) {
            for (int j = i; j < i + mh; j++) {
                int pos = j * 2;
                assert(pos < N2);
                float wr = cos(TWO_PI * (j - i) / m);
                float wi = -sin(TWO_PI * (j - i) / m);

                int indexRe1 = pos;
                int indexIm1 = pos + 1;

                int indexRe2 = pos + m;
                int indexIm2 = pos + m + 1;

                assert(indexRe1 < N2 && indexRe2 < N2);
                assert(indexIm1 < N2 && indexIm2 < N2);

                float re = array[indexRe2] * wr - array[indexIm2] * wi;
                float im = array[indexRe2] * wi + array[indexIm2] * wr;

                float temp_re = array[indexRe1];
                float temp_im = array[indexIm1];

                array[indexRe1] = temp_re + re;
                array[indexIm1] = temp_im + im;

                array[indexRe2] = temp_re - re;
                array[indexIm2] = temp_im - im;
            }
        }
    }
}

#define N 32
static float data[N*2];

int main() {
    for (int i = 0; i < N; i++) {
        data[i*2] = i;
        data[i*2+1] = 0;
    }
    for (int i = 0; i < N; i++) {
        cout << data[i*2] << " " << data[i*2+1] << "j, ";
    }
    cout << "\n\n";
    fft(data, N);
    cout.precision(3);
    for (int i = 0; i < N; i++) {
        cout << data[i*2] << " " << data[i*2+1] << "j, ";
    }
    std::cout << "\nDone!\n";
    return 0;
}
