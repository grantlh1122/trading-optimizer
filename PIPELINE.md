# Trading engine pipeline and file guide

This project is organized so the same downloaded dataset can be replayed for a
single backtest, an Optuna trial, or a future user-facing result screen. The
important rule is that no component guesses the current date or fetches more
data after a run has started.

## Recommended flow

```text
application startup
        |
        v
Alpaca adapter downloads completed bars once
        |
        v
market.py: MarketDataSnapshot (immutable, UTC timestamps)
        |
        +--> training.py: TrainingRunPlan
        |       - window: last completed bar minus N calendar years to last bar
        |       - ordered Optuna-selected parameter epochs
        |
        v
native adapter (future pybind11 boundary)
        |
        v
backtest.cpp: Backtester.run(candles, window, schedule, strategy)
        |
        v
fills, open trades, equity curve, Sharpe status -> result.py / future UI
```

`MarketDataSnapshot.last_complete_candle_end_utc` is the cutoff for the run.
For example, if the last fully closed downloaded bar ends at 2026-10-15 20:00
UTC and the user selects five years, the simulation window begins at the same
UTC calendar time on 2021-10-15. The data loader should download extra bars
before that boundary for moving-average warm-up. Those earlier bars are given
to the strategy but do not create fills or contribute to the equity curve.

This makes a run repeatable even if the program is launched tomorrow or an
Alpaca request later returns newer bars.

## Native file responsibilities

- `trading_types.hpp` is the shared, dependency-free vocabulary: candles,
  timestamps, signals, parameter sets, and the inclusive backtest window.
- `strategies.hpp` declares the abstract `Strategy` interface.
- `strategies.cpp` contains the moving-average crossover implementation. Add
  future signal-only strategies here or in separate strategy implementation
  files; do not put cash or fill code in them.
- `backtest.hpp` exposes the backtest input and output types.
- `backtest.cpp` owns account accounting, signal order queues, market-open
  fills, stop/take-profit exits, marks to market, and parameter schedules.
- `tests/backtest_test.cpp` is a fast native regression test for crossover,
  closed-market queuing, parameter epochs, and disabled risk exits.

The current project intentionally does not yet include a Python-to-C++ binding
or an Alpaca SDK adapter. I recommend a narrow pybind11 adapter as the next
integration step: translate the immutable Python snapshot into
`std::vector<Candle>`, call `Backtester.run`, then return plain result records.
Keeping that adapter thin means a future CLI, web API, or desktop interface
will all use exactly the same engine.

## One-candle event order

`Backtester.run` iterates the already-downloaded sequence in chronological
order. For each candle in the evaluation window it does the following:

1. If this is a tradable (`kOpen`) candle, fill any signal orders queued from
   earlier closes at this candle's open price.
2. If the market is open, evaluate the candle against each open long trade's
   stop loss and take profit.
3. Mark the account to that candle's close and add the completed candle to
   strategy history.
4. Call `Strategy::generate_signal(history, active_parameters)`.
5. Queue Buy or Sell for a later market open. Hold does nothing.

A completed candle's close, high, and low are unavailable until the candle has
finished. Therefore even a signal formed while the market is open is filled at
the next eligible candle open; this prevents a close-price look-ahead bias. If
a candle is marked `kClosed`, queued orders remain untouched until a later
`kOpen` candle. Typical Alpaca stock-bar downloads omit overnight and weekend
bars, so the next supplied regular-session bar is naturally the next open.

The engine is long-only. A Buy opens one aggregate long position when flat. A
Sell closes all open trades at the next eligible open; it does not open a
short. A newer opposite signal waiting during a closure cancels the older
queued signal, so the latest completed-close decision is followed.

The final candle does not force a liquidation. The result retains any open
position and any order still waiting for a market open, and its last equity
point marks the position at the final close.

## Stops, targets, and bar-data assumptions

`RiskParameters.take_profit_fraction` and
`RiskParameters.stop_loss_fraction` are decimal fractions of a trade's entry
price: `0.05` means five percent. A value of `0.0` explicitly disables that
exit. The backtester stores disabled levels as price `0.0` and never tests them
against a candle. This is the intended representation for Optuna trials that
choose zero.

For an enabled level, a gap through it fills at the candle open. An intrabar
touch fills at the stop or target price. If both levels fit within the same OHLC
candle, the path inside the bar is unknowable; the engine assumes the stop was
hit first, the conservative long-position outcome. This is documented behavior
that should be retained unless tick data or an intrabar execution model is
introduced.

No commissions, slippage, spread, corporate actions, dividends, borrowing,
margin, or short selling are modeled yet. Add them to the execution/accounting
layer rather than individual strategies so every strategy is evaluated under
the same assumptions.

## Walk-forward parameters and Optuna

`ParameterChange` has an effective candle-close timestamp. The latest entry
whose time is at or before a candle close is passed to the strategy for that
candle's signal. Changing parameters does not close a position or rewrite its
existing stop/target levels. Parameters active when a Buy is signaled travel
with the queued order, so a later parameter epoch cannot alter an order that
was already waiting for market open.

In `training.py`, create one `ParameterEpoch` for the initial Optuna-selected
parameters and one for each predetermined walk-forward boundary. Construct the
`TrainingRunPlan` once from the snapshot and requested number of years. Its
validation rejects a window extending past the last completed downloaded bar
and rejects unsorted parameter times.

For a moving-average strategy with long window `L`, request at least `L + 1`
warm-up candles before the evaluation start. The extra candle is needed to
compare the prior and current long moving averages and detect a real cross.

## Path toward a user experience

Keep the calculation pipeline free of UI state. A practical next layout is:

```text
market.py                 # Alpaca adapter + immutable snapshot contract
training.py               # trial/window/schedule construction
native_adapter.py         # thin pybind11 conversion boundary
backtest.cpp, strategies.cpp
result.py                 # stable result view-model / serialization
app.py or api.py          # user actions, progress, validation, presentation
```

The UI should ask for a symbol, bar interval, historical years, cash
allocation, and optimization ranges. It should then display the snapshot's
exact data range and the evaluated window before a run starts. Persist the
symbol, provider request settings, snapshot cutoff, window timestamps,
parameter schedule, fills, and engine version with each result. That metadata
lets a user reopen a result and understand precisely what data and assumptions
produced it.

## Build and verify

Run `make test` to compile the native engine with C++17 warnings treated as
errors and execute the deterministic regression tests. The test includes a Buy
signal during closed candles and verifies that it fills only at the next open,
as well as a zero stop/take-profit run that leaves the position unprotected.
