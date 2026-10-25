# Observed results and limits

Historical measurements: 2026-10-25, one physically VRAM-modified Leadtek RTX 2080 Ti (stock 11 GB → 22 GB, reporting 22 GiB) unlocked by a one-shot Linux/initramfs experiment. The other same-model, also 22 GB VRAM-modded card remained compute-locked. The memory modifications predate this experiment; the software adds no VRAM. Kernel 6.8.0-147, NVIDIA open 595.99.02; power limit 250 W on both. Original record retained privately; public tables are summarized measurements, not complete raw logs.

## Same-card mmapeak A/B

Short instruction tests, target 0.3 s each. Same physical GPU selected by verified UUID before/after. Clocks not locked or normalized, so ratios are measured throughput, not exact per-clock rates.

| Instruction | Locked | Unlocked | Unit |
|---|---:|---:|---|
| FP16→FP32 16×16×16 | 59.9 | 110.9 | TFLOPS |
| FP16→FP32 32×8×16 | 59.9 | 111.0 | TFLOPS |
| FP16→FP16 16×16×16 | 119.5 | 118.5 | TFLOPS |
| FP16→FP16 32×8×16 | 119.5 | 118.5 | TFLOPS |
| INT8→INT32 16×16×16 | 238.6 | 236.5 | TOPS |
| INT8→INT32 32×8×16 | 236.8 | 236.6 | TOPS |

Approximately +85% on the targeted FP32-accumulating path; controls essentially flat. Native BF16/TF32 remained unsupported.

## Original MAMF engine, two-card comparison

FP16, PyTorch 2.8.0+cu129, 30 s initial warmup, 50 warmup + 100 measured iterations per shape. Original transposed-B layout, cache/destination-reset behavior, timing function retained; terminal UI changed only. No claim of strict same-card baseline. Silicon/cooling/clock differences between cards remain possible.

Manual sweep: M/N/K independently 2048..6144 inclusive, step 1024, 125 shapes. Best locked median 57.71 TFLOPS at 2048×2048×3072; best unlocked median 88.24 at 2048×2048×4096, +52.9% across best candidates.

| Shape | Locked median | Unlocked median | Unit |
|---|---:|---:|---|
| 2048×2048×2048 | 57.21 | 87.83 | TFLOPS |
| 2048×2048×3072 | 57.71 | 86.98 | TFLOPS |
| 2048×2048×4096 | 57.66 | 88.24 | TFLOPS |
| 2048×3072×2048 | 57.00 | 86.52 | TFLOPS |
| 2048×3072×3072 | 56.80 | 85.40 | TFLOPS |
| 2048×3072×4096 | 56.70 | 85.89 | TFLOPS |
| 2048×2048×6144 | 57.07 | 84.52 | TFLOPS |

Separate single-shape runs: 4096³ locked/unlocked median 54.34/76.99; 8192³ 55.33/76.15. Do not mix them with sweep scores as if conditions/order were identical.

## Ampere

Four 3090s, sequential PyTorch 8192³ tests at 360 W: FP16 122.76/122.96/127.40/115.21; BF16 126.57/127.45/130.39/118.39 TFLOPS. Numerical sanity checks passed in that run. An early baseline used the wrong GPU selector, so we do not report a same-card before/after percentage for those four cards.

## What is NOT established

Turing numerical correctness has not been independently certified by the local public-repo measurement record; the recorded runs here are throughput tests and healthy-driver checks. Upstream's own correctness results apply to its hardware/profile, not automatically this port. A local validation script was drafted during development but its successful execution was not recorded; do not turn that into a pass claim. Independent numerical, memory-integrity and sustained thermal/endurance testing remains required.

No deliberate post-launch hardware failure recovery test, long-duration stability certification, game/LLM speedup measurement, or successful second-card TU102 unlock is claimed. Host simulation, matching hashes and successful boot are valuable but narrower evidence.
