# Animation benchmark: after-vsync-off

- Timestamp: 2026-11-01T23:21:54.112325177Z
- XDG_SESSION_TYPE: wayland
- Prism pipeline/order: es2,sw
- Effective VSync: false (source: default)
- JavaFX pulse logger: true

Each scenario used 3 discarded warmups, then 15 measured iterations. The first row of statistics is measured iteration 1 after warmup; steady state aggregates measured iterations 2–15. Frame times are pulse intervals sampled with an AnimationTimer. Setup time includes synchronous trigger work, applyCss(), and layout() on the FX thread.

| Animation | Run | Frames | Avg ms | p95 ms | Max ms | >16.6 ms | Trigger ms | CSS ms | Layout ms | Setup ms |
|---|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| Screen switch | First run | 43 | 16.13 | 16.31 | 16.40 | 0.00% | 1.00 | 0.95 | 0.46 | 2.41 |
| Screen switch | Steady state | 606 | 16.18 | 16.25 | 23.41 | 1.49% | 2.27 | 5.12 | 0.91 | 8.29 |
| Flyout · instance settings | First run | 40 | 18.01 | 16.22 | 92.08 | 2.50% | 20.82 | 50.08 | 14.43 | 85.33 |
| Flyout · instance settings | Steady state | 571 | 17.11 | 16.35 | 77.58 | 2.80% | 11.47 | 33.35 | 6.33 | 51.15 |
| Flyout · launcher settings | First run | 40 | 16.13 | 16.27 | 16.34 | 0.00% | 2.54 | 4.39 | 1.93 | 8.86 |
| Flyout · launcher settings | Steady state | 563 | 16.17 | 16.25 | 32.91 | 0.71% | 4.15 | 5.66 | 1.63 | 11.44 |
| Flyout · accounts | First run | 40 | 16.11 | 16.23 | 16.33 | 0.00% | 0.90 | 4.33 | 1.42 | 6.65 |
| Flyout · accounts | Steady state | 560 | 16.11 | 16.25 | 16.42 | 0.00% | 0.99 | 4.55 | 0.88 | 6.41 |
| Tab switch | First run | 43 | 16.12 | 16.24 | 16.42 | 0.00% | 0.47 | 2.25 | 0.31 | 3.03 |
| Tab switch | Steady state | 601 | 16.14 | 16.25 | 26.46 | 0.50% | 0.76 | 2.49 | 0.49 | 3.75 |
| Card hover | First run | 26 | 16.13 | 16.27 | 16.37 | 0.00% | 0.73 | 0.73 | 0.79 | 2.25 |
| Card hover | Steady state | 363 | 16.13 | 16.27 | 20.13 | 0.28% | 1.01 | 1.05 | 1.01 | 3.08 |
| Toast | First run | 40 | 16.12 | 16.29 | 16.39 | 0.00% | 0.50 | 0.78 | 0.51 | 1.79 |
| Toast | Steady state | 559 | 16.14 | 16.27 | 26.60 | 0.89% | 0.53 | 0.90 | 0.31 | 1.74 |
| Startup animation | First run | 52 | 16.11 | 16.24 | 16.36 | 0.00% | 0.31 | 0.60 | 0.00 | 0.91 |
| Startup animation | Steady state | 727 | 16.14 | 16.24 | 29.76 | 0.28% | 0.24 | 0.41 | 0.00 | 0.65 |
