# Animation benchmark: after-vsync-on

- Timestamp: 2026-11-01T23:25:27.663179858Z
- XDG_SESSION_TYPE: wayland
- Prism pipeline/order: es2,sw
- Effective VSync: true (source: command-line override)
- JavaFX pulse logger: true

Each scenario used 3 discarded warmups, then 15 measured iterations. The first row of statistics is measured iteration 1 after warmup; steady state aggregates measured iterations 2–15. Frame times are pulse intervals sampled with an AnimationTimer. Setup time includes synchronous trigger work, applyCss(), and layout() on the FX thread.

| Animation | Run | Frames | Avg ms | p95 ms | Max ms | >16.6 ms | Trigger ms | CSS ms | Layout ms | Setup ms |
|---|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| Screen switch | First run | 82 | 8.46 | 16.26 | 16.37 | 0.00% | 1.22 | 0.97 | 0.32 | 2.51 |
| Screen switch | Steady state | 999 | 9.77 | 16.24 | 90.61 | 2.30% | 2.00 | 3.54 | 0.68 | 6.23 |
| Flyout · instance settings | First run | 74 | 9.70 | 16.17 | 88.93 | 1.35% | 29.06 | 44.89 | 8.76 | 82.70 |
| Flyout · instance settings | Steady state | 1056 | 9.37 | 16.22 | 97.22 | 1.80% | 12.64 | 42.80 | 7.22 | 62.65 |
| Flyout · launcher settings | First run | 92 | 7.18 | 16.16 | 16.24 | 0.00% | 3.14 | 6.35 | 3.11 | 12.61 |
| Flyout · launcher settings | Steady state | 1261 | 7.26 | 16.16 | 37.34 | 0.48% | 3.54 | 6.56 | 1.63 | 11.73 |
| Flyout · accounts | First run | 88 | 7.36 | 16.18 | 19.00 | 1.14% | 1.10 | 8.12 | 1.71 | 10.93 |
| Flyout · accounts | Steady state | 1294 | 6.99 | 16.12 | 51.72 | 0.15% | 1.07 | 4.51 | 0.72 | 6.30 |
| Tab switch | First run | 68 | 10.20 | 16.10 | 98.67 | 1.47% | 0.38 | 1.83 | 0.38 | 2.59 |
| Tab switch | Steady state | 1340 | 7.24 | 16.14 | 132.74 | 0.37% | 0.60 | 2.15 | 0.49 | 3.24 |
| Card hover | First run | 46 | 8.90 | 16.11 | 16.24 | 0.00% | 1.67 | 1.31 | 1.88 | 4.86 |
| Card hover | Steady state | 648 | 9.03 | 16.20 | 77.96 | 0.62% | 1.51 | 1.31 | 1.28 | 4.09 |
| Toast | First run | 93 | 7.00 | 16.08 | 55.85 | 1.08% | 0.68 | 1.12 | 0.50 | 2.30 |
| Toast | Steady state | 1396 | 6.45 | 16.15 | 58.78 | 0.43% | 0.51 | 0.71 | 0.27 | 1.49 |
| Startup animation | First run | 141 | 5.94 | 16.06 | 16.21 | 0.00% | 0.31 | 0.62 | 0.00 | 0.94 |
| Startup animation | Steady state | 1787 | 6.58 | 16.09 | 118.94 | 0.73% | 0.23 | 0.42 | 0.00 | 0.65 |
