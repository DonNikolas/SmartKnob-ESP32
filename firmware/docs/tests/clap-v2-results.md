# Experimental FFT clap rules — 2026-10-28

The detector combines onset RMS >= max(1500, 6*background), frame rise >=4,
crest >=3, low-band fraction <=0.25, mid >=0.50 and high <=0.40.
Low is 125..<500 Hz, mid 500..<2000 Hz, high 2000..7500 Hz.
The signal must decay below 45% of its peak for two frames within 112 ms.
Two accepted onsets must be 160..600 ms apart, with 700 ms cooldown after a pair.
The nominal frame duration is 16 ms. No lamp or motor actions are implemented.

These are provisional heuristic limits based on this enclosure and the supplied
recordings. They are not a trained classifier or a universal clap signature.
In particular, the upper high-band limit could reject claps in other rooms or
positions; it must be checked with new recordings rather than assumed reliable.

Replay with the actual C++ detector:

| Recording | Frames | Pair detections |
| --- | ---: | ---: |
| log-klatschen.txt | 963 | 5 |
| log-reden.txt | 625 | 0 |
| log-mechanisch.txt | 589 | 0 |

All ten clap impulses were accepted, forming the five expected pairs.
These same recordings informed the rules, so these results are not independent
validation and do not establish a false-positive rate. CSV values are rounded;
firmware uses full precision. Replay uses existing FFT features, not raw audio.

Synthetic checks cover sustained noise rejection, one pair, cooldown,
missing-frame reset and clipped onset rejection. Read errors, recalibration,
receive-slot changes and phase changes reset detection in firmware.

Build/run the host check from firmware on a host with g++:

```sh
g++ -std=c++11 -Wall -Wextra -Werror docs/tests/clap-v2-host.cpp -o clap-v2-host
./clap-v2-host path/to/log-klatschen.txt path/to/log-reden.txt path/to/log-mechanisch.txt
```

No PlatformIO toolchain was available for a complete ESP32 build here.
Real-device compilation and new recordings remain required before GUI integration.
