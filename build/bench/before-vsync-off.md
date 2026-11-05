# Animation benchmark: before-vsync-off

- Timestamp: 2026-11-01T22:43:23.692329765Z
- XDG_SESSION_TYPE: wayland
- Prism pipeline/order: es2,sw
- Effective VSync: false (source: default)
- JavaFX pulse logger: true

Each scenario used 3 discarded warmups, then 15 measured iterations. The first row of statistics is measured iteration 1 after warmup; steady state aggregates measured iterations 2–15. Frame times are pulse intervals sampled with an AnimationTimer. Setup time includes synchronous trigger work, applyCss(), and layout() on the FX thread.

| Animation | Run | Frames | Avg ms | p95 ms | Max ms | >16.6 ms | Trigger ms | CSS ms | Layout ms | Setup ms |
|---|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| Screen switch | First run | 43 | 16.10 | 16.26 | 16.33 | 0.00% | 2.68 | 2.03 | 0.40 | 5.11 |
| Screen switch | Steady state | 588 | 16.76 | 21.89 | 50.71 | 7.99% | 7.02 | 3.30 | 2.32 | 12.64 |
| Flyout · instance settings | First run | 40 | 17.81 | 16.33 | 84.35 | 2.50% | 7.61 | 60.54 | 7.97 | 76.12 |
| Flyout · instance settings | Steady state | 569 | 17.26 | 16.32 | 96.65 | 3.16% | 6.37 | 44.31 | 7.04 | 57.72 |
| Flyout · launcher settings | First run | 41 | 16.53 | 16.29 | 33.29 | 2.44% | 3.17 | 18.38 | 6.51 | 28.06 |
| Flyout · launcher settings | Steady state | 570 | 17.04 | 16.29 | 76.00 | 2.81% | 4.96 | 35.95 | 7.29 | 48.20 |
| Flyout · accounts | First run | 40 | 16.39 | 16.30 | 27.01 | 2.50% | 1.06 | 4.73 | 1.25 | 7.04 |
| Flyout · accounts | Steady state | 559 | 16.15 | 16.26 | 29.37 | 0.54% | 0.87 | 4.40 | 1.05 | 6.32 |
| Tab switch | First run | 43 | 16.12 | 16.23 | 16.34 | 0.00% | 0.51 | 1.55 | 0.24 | 2.30 |
| Tab switch | Steady state | 602 | 16.11 | 16.24 | 16.38 | 0.00% | 0.71 | 2.66 | 0.41 | 3.78 |
| Card hover | First run | 26 | 16.11 | 16.22 | 16.32 | 0.00% | 0.72 | 1.47 | 0.90 | 3.09 |
| Card hover | Steady state | 363 | 16.15 | 16.24 | 30.20 | 0.55% | 1.06 | 1.38 | 1.27 | 3.71 |
| Toast | First run | 40 | 16.11 | 16.21 | 16.34 | 0.00% | 0.34 | 0.54 | 0.20 | 1.08 |
| Toast | Steady state | 559 | 16.13 | 16.24 | 28.44 | 0.18% | 0.48 | 0.81 | 0.28 | 1.57 |
| Startup animation | First run | 52 | 16.11 | 16.20 | 16.27 | 0.00% | 0.31 | 0.57 | 0.00 | 0.89 |
| Startup animation | Steady state | 726 | 16.16 | 16.23 | 41.10 | 0.28% | 0.25 | 0.47 | 0.00 | 0.72 |
