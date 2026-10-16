# Trading Optimizer

A timestamp-aware, long-only backtesting core for evaluating technical
strategies against an already-downloaded market-data snapshot.

The first implemented strategy is a moving-average crossover. Strategy code is
kept separate from execution and accounting: it only emits Buy, Sell, or Hold
from historical completed candles. The backtester owns order timing, account
cash, protective exits, equity, and walk-forward parameter changes.

## What is implemented

- Abstract C++ `Strategy` interface plus moving-average crossover logic in
  `strategies.cpp`.
- One-candle-at-a-time simulation in `backtest.cpp`.
- Next-market-open fills for close-derived signals; closed-market signals stay
  queued until a tradable candle arrives.
- Long-only sizing, whole shares, explicit fills, open-trade state, equity
  curve, and Sharpe-ratio status.
- Take-profit and stop-loss fractions where `0.0` means disabled, including
  when parameters originated in an Optuna trial.
- Explicit UTC dataset and backtest-window contracts in `market.py` and
  `training.py`; the simulated end comes from the last complete downloaded bar,
  never the host clock.
- Deterministic C++ regression tests.

## Build

```bash
make test
```

## Design guide

Read [PIPELINE.md](PIPELINE.md) for the full data flow, execution sequence,
time-window rule, parameter-schedule behavior, OHLC assumptions, and a
recommended path to an Alpaca adapter, Optuna integration, and user-facing
application.

## Current limits

There is no Alpaca SDK adapter, Python/C++ binding, user interface yet.
