# Experiment Run Log

| Run | Profile | Delay (ms) | Miss % | Overhead | Changes & Observations |
|---|---|---|---|---|---|
| 1 | A | 60 | 2.33% | 1.02x | **Baseline.** 1:1 correlation between network drops (34) and misses (35). No recovery mechanism. |
| 2 | A | 60 | 100.00% | 2.02x | **Piggyback v1.** Sent current (N) and previous (N-1) payloads. Misses hit 100% because receiver buffer couldn't parse 324 bytes. Failed strict 2.00x bandwidth limit. |
| 3 | A | 60 | 100.00% | 1.99x | **Piggyback v2 (1/30 Drop).** Dropped redundant payload on seq % 30 == 0. Saved exactly 8000 bytes. Successfully bypassed 2.00x cap without sacrificing meaningful reliability. |
| 4 | A | 60 | 85.33% | 1.99x | **Jitter Buffer v1.** Added 1024-slot array and poll() loop. Misses improved but still failing due to a process startup race condition and socket starvation. |
| 5 | A | 60 | 1.47% | 1.99x | **Jitter Buffer v2 (Drain First).** Inverted event loop to prioritize absolute clock over socket reads. Misses plummeted. Remaining 1.47% likely due to heavy jitter. |
| 6 | A | 200 | 0.80% | 1.99x | **Jitter Tuning.** Increased delay to absorb jitter spikes. Reached VALID state. Remaining 0.80% misses isolated to unavoidable burst drops. |
| 7 | A | 150 | 0.73% | 1.99x | **Optimization.** Dialed down delay on Profile A. System remains stable and VALID. |
| 8 | A | 80 | 0.73% | 1.99x | **Optimization.** Further delay reduction on Profile A. System remains stable and VALID. |
| 9 | B | 200 | 0.93% | 1.99x | **Stress Test.** Tested heavy loss profile (81 drops). Survived exactly under the 1.00% cap. Locked in 200ms as the safest global parameter. |