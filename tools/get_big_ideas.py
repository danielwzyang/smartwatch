# downloads one night (3 pm to 3 pm) of one person from the dataset
# https://physionet.org/content/big-ideas-glycemic-wearable/
# saves test_hr.csv and test_angle.csv

import io
import os
import urllib.request
import numpy as np
import pandas as pd

person = "001" # 001 to 016
night = "2020-02-14"

url = f"https://physionet.org/files/big-ideas-glycemic-wearable/1.1.3/{person}"
folder = f"data/big_ideas/{person}/{night}"
start = pd.Timestamp(night) + pd.Timedelta(hours=15)
end = start + pd.Timedelta(hours=24)

os.makedirs(folder, exist_ok=True)

# downloads bytes lo to hi of a file
def fetch(file, lo=0, hi=0):
    request = urllib.request.Request(f"{url}/{file}", headers={
        "Range": f"bytes={lo}-{"" if hi == 0 else hi}"
    })

    response = urllib.request.urlopen(request)

    return response.read().decode()

print("downloading heart rate")
text = fetch(f"HR_{person}.csv")
hr = pd.read_csv(io.StringIO(text), names=["time", "bpm"], header=0)
hr.time = pd.to_datetime(hr.time, format="%m/%d/%y %H:%M")

# each minute has multiple readings so group them together and average the readings
minutes = hr.groupby("time").bpm
avg_bpm = minutes.mean()
valid_seconds = minutes.count()

# list of every minute (including those that have no reading)
every_minute = pd.date_range(start, end, freq="min", inclusive="left")
# 0 = no readings for that minute
avg_bpm = avg_bpm.reindex(every_minute, fill_value=0)
valid_seconds = valid_seconds.reindex(every_minute, fill_value=0)

with open(f"{folder}/test_hr.csv", "w") as f:
    f.write("seconds_since_boot,avg_bpm,valid_seconds\n")
    f.write("boot,,\n")
    for i in range(len(every_minute)):
        seconds = (i + 1) * 60
        f.write(f"{seconds},{avg_bpm.iloc[i]:.1f},{valid_seconds.iloc[i]}\n")

hours = (valid_seconds > 0).sum() / 60
print(f"saved test_hr.csv ({hours:.1f} / 24 hours with heart rate)")

# the accelerometer file is ~900 MB but sorted by time,
# so binary search for where the night starts and ends and only download that
acc_file = f"ACC_{person}.csv"
request = urllib.request.Request(f"{url}/{acc_file}", method="HEAD")
size = int(urllib.request.urlopen(request).headers["Content-Length"])

def find(when):
    lo = 0
    hi = size - 300
    while hi - lo > 4096:
        mid = (lo + hi) // 2
        # the first full line after mid
        lines = fetch(acc_file, mid, mid + 200).split("\n")
        time = pd.Timestamp(lines[1].split(",")[0])
        if time < when:
            lo = mid
        else:
            hi = mid
    return lo

print("finding the night in the accelerometer file")
lo = find(start)
hi = find(end) + 4096
print(f"downloading accelerometer: {(hi - lo) / 1e6:.0f} MB")
text = fetch(acc_file, lo, hi)

# drop the cut-off first and last lines
first = text.index("\n") + 1
last = text.rindex("\n")
text = text[first:last]

acc = pd.read_csv(io.StringIO(text), names=["time", "x", "y", "z"], index_col=0)
acc.index = pd.to_datetime(acc.index, format="ISO8601")

# wrist angle (how far z tilts up or down from the xy plane), for every sample
angle = np.degrees(np.arctan2(acc.z, np.sqrt(acc.x ** 2 + acc.y ** 2)))

# average of every 5 second interval
avg_angle = angle.resample("5s").mean()
night_epochs = pd.date_range(start, end, freq="5s", inclusive="left")
avg_angle = avg_angle.reindex(night_epochs)

# where the wristband was off, repeat the last angle (like our watch sitting still on a table)
avg_angle = avg_angle.ffill().bfill()

with open(f"{folder}/test_angle.csv", "w") as f:
    f.write("seconds_since_boot,avg_angle\n")
    f.write("boot,\n")
    for i in range(len(night_epochs)):
        seconds = (i + 1) * 5
        f.write(f"{seconds},{avg_angle.iloc[i]:.1f}\n")

hours = avg_angle.notna().sum() * 5 / 3600
print(f"saved test_angle.csv ({hours:.1f} / 24 hours with accelerometer data)")

