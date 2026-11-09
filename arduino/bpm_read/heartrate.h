// paper: https://doi.org/10.3390/s20061783
// .h to be treated like library

#pragma once

#include <math.h>
#include <string.h>
#include <algorithm>

namespace hr {
    // sampling rate that we choose for max30101
    const int RATE = 50;

    // window length = 500 samples = 10 seconds
    const int N = 500;

    // healthy adults sleep around 40-60 bpm
    const int MIN_BPM = 40;

    // largest allowed lambda
    // a lower lambda max rejects more junk but reduces the minimum bpm that can be measured
    // lambda is roughly half a heartbeat for a perfect sin wave
    // ^ is a claim from claude derived from the paper's definition, tested on sine waves
    // slowest beat will last for RATE * 60 / MIN_BPM samples
    // LAMBDA_MAX = ceil(RATE * 60 / (2 * MIN_BPM)); 2 on bottom bc 1/2 of heartbeat
    // a = RATE * 60, b = 2 * MIN_BPM
    // integer math: ceil(a / b) = (a + b - 1) / b
    const int LAMBDA_MAX = (RATE * 60 + 2 * MIN_BPM - 1) / (2 * MIN_BPM);

    // stretches of movement that are shorter than this get ignored
    // 25 samples at 50 Hz = 0.5s (0.5s from paper)
    const int BLIP = 25;

    // comes from paper
    // may need testing for 50 Hz
    const float MOVE_LIMIT = 0.0025;

    // brightness of LED
    // found that 0x7F is pretty accurate for wrist readings
    const int BRIGHTNESS = 0x7F;

    // lower limit = -LIMIT, upper limit = +LIMIT
    // ~100-150% of filtered pulse's amplitude
    // tuned based on brightness
    const float LIMIT = 150;

    // minimum ppg reading to count as contact
    // tuned based on brightness
    // no contact is typically less than this value during testing but value isnt bulletproof
    const float CONTACT_MIN = 10000;

    // scales movement into range 0 to 1
    // accelerometer can be set to +/- 2g since we only need to detect movement is happening
    // maximum jump for each axis is 4g
    // maximum jump in magnitude (3 axes) is sqrt(3 * 4**2) = 4sqrt(3) = 6.93
    const float G = 6.93;

    const int MIN_PEAKS = N * MIN_BPM / (60 * RATE);
        // minimum peaks = floor(N * MIN_BPM / (60 * RATE)) from paper

    struct Biquad {
        float b0, b1, b2, a1, a2;
        // previous inputs x_{i-1} and x_{i-2}
        // previous outputs y_{i-1} and y_{i-2}
        float x1 = 0, x2 = 0, y1 = 0, y2 = 0;
        
        // runs sample through filter and limits the output
        // fed back into the next pass
        // lim = INFINITY -> no clamping (used for 2nd stage of biquads)
        float run(float x, float lim = INFINITY) {
            float y = fmaxf(-lim, fminf(lim, b0 * x + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2));

            // update stored previous inputs and outputs
            x2 = x1;
            x1 = x;
            y2 = y1;
            y1 = y;

            return y;
        }
    };

    // see `tools/bandpass.py`
    Biquad stage1{0.112160244f, 0, -0.112160244f, -1.758090413f, 0.775679511f};
    Biquad stage2a{0.013359200f, 0.026718400f, 0.013359200f, -1.686278257f, 0.753714473f};
    Biquad stage2b{1, -2, 1, -1.925051595f, 0.929923474f};

    float filtered_ppg[N];
    bool moving[N];

    // stops counting at N
    int samples_taken = 0;
    
    // green = raw ppg value
    // x, y, z = acceleration
    void addSample(float green, float x, float y, float z) {
        // need to save previous accel sample
        // set to the first sample with `static`
        static float px = x, py = y, pz = z;

        // step 1: clean the signal
        // run stage1 biquad with limiter then stage2 2 biquads
        float filtered = stage2b.run(
            stage2a.run(
                stage1.run(green, LIMIT)));
        
        // step 2: mark as moving
        float movement = sqrtf((x - px) * (x - px) + (y - py) * (y - py) + (z - pz) * (z - pz)) / G;
        px = x;
        py = y;
        pz = z;

        // slide windows to the left by 1
        // memmove(dest, src, count)
        memmove(filtered_ppg, filtered_ppg + 1, (N - 1) * sizeof(float));
        memmove(moving, moving + 1, N - 1);

        filtered_ppg[N - 1] = filtered;
        moving[N - 1] = movement > MOVE_LIMIT;

        // make sure window only contains valid samples where skin is touching
        if (green < CONTACT_MIN) samples_taken = 0;
        else if (samples_taken < N) samples_taken++;
    }

    // 0 if filtered_ppg[i] greater than both neighbors k samples away (local max at scale k)
    // otherwise 1
    int compare_kth_neighbors(int i, int k) {
        return i - k < 0 || // neighbor to the left doesn't exist
            i + k >= N || // neighbor to the right doesn't exist
            filtered_ppg[i] <= filtered_ppg[i - k] || // not greater than left neighbor
            filtered_ppg[i] <= filtered_ppg[i + k]; // not greater than right neighbor
    }

    float getBPM() {
        if (samples_taken < N) return 0; // window not big enough

        // using static so it doesn't live on the stack
        // loop() and everything it calls share an 8kb stack (pretty small)
        static float peaks[N], gaps[N];
        static int comparison_sums[N];
        static bool cleaned_moving[N];

        // step 1: find the peaks using modified ampd algorithm from paper
        // k goes from 1 to L, L = ceil(N / 2) - 1
        // ceil(N / 2) = (N + 1) / 2 for integers
        int L = (N + 1) / 2 - 1;

        // k with the smallest gamma
        // gamma is at most N if every sample returns 1
        int lambda = 1, smallest_gamma = N + 1;

        for (int k = 1; k <= L; k++) {
            int gamma = 0;
            for (int i = 0; i < N; i++) gamma += compare_kth_neighbors(i, k);

            if (gamma < smallest_gamma) {
                smallest_gamma = gamma;
                lambda = k;
            }
        }

        if (lambda > LAMBDA_MAX) return 0;

        // comparison_sums[i] = sum of comparisons for k from 1 to lambda
        for (int i = 0; i < N; i++) {
            comparison_sums[i] = 0;
            for (int k = 1; k <= lambda; k++)
                comparison_sums[i] += compare_kth_neighbors(i, k);
        }

        int num_peaks = 0;
        for (int i = 0; i < N; i++) {
            // s[i] = 0 means every comparison passes so it's a valid peak
            if (comparison_sums[i] == 0) peaks[num_peaks++] = i;

            // if there's a flat peak (two equal samples at the peak)
            // s[i - 2] > 1, s[i - 1] = 1, s[i] = 1, s[i + 1] > 1
            // need to make sure i - 2 and i + 1 are inside the window
            // peak happens in between the two equal samples so we estimate as the middle
            if (i >= 2 && i < N - 1 && 
                    comparison_sums[i - 2] > 1 && comparison_sums[i - 1] == 1 && 
                    comparison_sums[i] == 1 && comparison_sums[i + 1] > 1)
                peaks[num_peaks++] = i - 0.5f;
        }

        // flat peaks can cause the peaks to be out of order so we sort so each peak is increasing in time
        std::sort(peaks, peaks + num_peaks);

        // step 2: look at moving and remove any movement stretches that are shorter than BLIP
        // if there's a small stretch of movement like a tiny wrist twitch the heartrate can still be reliable
        // do it on a copy because the stretch at the end of a window might be long enough in the next window
        memcpy(cleaned_moving, moving, N);
        for (int i = 0; i < N; ) {
            int j = i;
            // j keeps walking until the stretch of movement that starts at i ends
            // if there's no movement then j = i
            while (j < N && cleaned_moving[j]) j++;
            
            // memset(ptr, value, n)
            if (j - i < BLIP) memset(cleaned_moving + i, 0, j - i);
            
            i = j + 1;
        }

        // step 3: calculate bpm
        int num_gaps = 0;
        for (int peak = 1; peak < num_peaks; peak++) {
            // check whether there's any movement between two neighboring peaks
            // for each sample check from last peak to next peak
            // flat peaks sit between samples (e.g. 100.5), so go one past to make sure the sample after is checked
            bool movement = false;
            for (int i = (int) peaks[peak - 1]; i <= peaks[peak] + 1 && i < N; i++)
                movement |= cleaned_moving[i]; // if any sample is moving then there is movement in this interval
            
            // if there's no movement then we save the time between the peaks (in seconds)
            if (!movement) gaps[num_gaps++] = (peaks[peak] - peaks[peak - 1]) / RATE;
        }

        // we need a certain number of good peaks, and every pair of peaks is one gap
        // thus we need  MIN_PEAKS - 1 gaps
        if (num_gaps < MIN_PEAKS - 1) return 0;

        // need to grab the median
        std::sort(gaps, gaps + num_gaps);

        // if num_gaps is odd, we grab the middle element
        // if it's even we grab the average of the middle two
        float median = num_gaps % 2 ? gaps[num_gaps / 2] : (gaps[num_gaps / 2 - 1] + gaps[num_gaps / 2]) / 2;

        // median is in seconds, bpm is (60 seconds / minute) / (median seconds)
        return 60 / median;
    }
}

