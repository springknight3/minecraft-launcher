# Animation benchmark: before-profile

- Timestamp: 2026-11-01T22:45:10.911250335Z
- XDG_SESSION_TYPE: wayland
- Prism pipeline/order: es2,sw
- Effective VSync: false (source: default)
- JavaFX pulse logger: true

Each scenario used 3 discarded warmups, then 15 measured iterations. The first row of statistics is measured iteration 1 after warmup; steady state aggregates measured iterations 2–15. Frame times are pulse intervals sampled with an AnimationTimer. Setup time includes synchronous trigger work, applyCss(), and layout() on the FX thread.

| Animation | Run | Frames | Avg ms | p95 ms | Max ms | >16.6 ms | Trigger ms | CSS ms | Layout ms | Setup ms |
|---|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| Screen switch | First run | 43 | 16.11 | 16.30 | 16.35 | 0.00% | 1.70 | 1.79 | 0.78 | 4.27 |
| Screen switch | Steady state | 595 | 16.49 | 16.33 | 48.91 | 4.20% | 2.61 | 6.09 | 1.05 | 9.75 |
| Flyout · instance settings | First run | 40 | 17.80 | 16.26 | 84.62 | 2.50% | 10.19 | 51.36 | 11.79 | 73.34 |
| Flyout · instance settings | Steady state | 569 | 17.36 | 16.30 | 88.58 | 2.46% | 6.63 | 46.87 | 8.03 | 61.53 |
| Flyout · launcher settings | First run | 41 | 16.96 | 16.23 | 51.31 | 2.44% | 4.23 | 38.15 | 5.84 | 48.22 |
| Flyout · launcher settings | Steady state | 570 | 16.95 | 16.29 | 81.85 | 2.63% | 4.20 | 33.38 | 7.71 | 45.30 |
| Flyout · accounts | First run | 40 | 16.13 | 16.28 | 16.34 | 0.00% | 0.75 | 3.90 | 0.94 | 5.59 |
| Flyout · accounts | Steady state | 561 | 16.11 | 16.25 | 16.43 | 0.00% | 1.09 | 5.59 | 1.11 | 7.79 |
