# prints the biquad coefficients used in bpm algorithm

from scipy.signal import butter

# standard filter is butterworth
# stage 1 = one biquad
# stage 2 = two biquads
for name, order in [("stage1", 1), ("stage2", 2)]:
    # 0.5 - 2 Hz, 50 Hz sampling rate, sos = each element in output array is a filter and its coefficients
    filters = butter(order, [0.5, 2.5], btype="bandpass", fs=50, output="sos")
    
    for b0, b1, b2, a0, a1, a2 in filters:
        print(f"{name}: {b0:.9f}f, {b1:.9f}f, {b2:.9f}f, {a1:.9f}f, {a2:.9f}f")
