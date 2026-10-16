/*
 * Shared, dependency-free types for the native trading engine.
 *
 * All timestamps are UTC Unix seconds.  The Python data boundary converts its
 * timezone-aware datetimes to this representation before calling native code.
 * Keeping a timestamp on every candle, configuration boundary, order, and
 * equity point makes a backtest reproducible: it never consults the computer's
 * current time or guesses which session a missing candle belongs to.
 */

#pragma once

#include <cstddef>
#include <cstdint>

namespace trading_optimizer {

using Timestamp = std::int64_t;

/*
 * Whether an order may be filled during this candle.  Alpaca bar downloads
 * normally contain only tradable-session bars, so they will usually all be
 * kOpen.  kClosed is useful when an upstream calendar supplies explicit
 * closed-session placeholders.  The engine does not infer this from gaps in
 * timestamps because holidays, overnight sessions, and asset classes differ.
 */
enum class MarketStatus { kOpen, kClosed };

/*
 * One fully completed OHLC candle for one symbol.
 *
 * open_timestamp_unix_seconds is the start of the bar and
 * close_timestamp_unix_seconds is its end.  A strategy sees a candle only
 * after it has closed, so it can safely use the complete high, low, and close
 * without accidentally looking into the future.
 */
struct Candle {
  Timestamp open_timestamp_unix_seconds = 0;
  Timestamp close_timestamp_unix_seconds = 0;
  double open_price_dollars = 0.0;
  double high_price_dollars = 0.0;
  double low_price_dollars = 0.0;
  double close_price_dollars = 0.0;
  MarketStatus market_status = MarketStatus::kOpen;
};

enum class SignalAction { kHold, kBuy, kSell };

/*
 * A strategy decision, made at the close of its most recently supplied
 * candle.  Signals deliberately contain no price or share count: execution
 * belongs to the backtester so a strategy cannot use a future opening price.
 */
struct StrategySignal {
  SignalAction action = SignalAction::kHold;
};

/* Parameters used by MovingAverageCrossoverStrategy. */
struct MovingAverageParameters {
  std::size_t short_window_candles = 10;
  std::size_t long_window_candles = 30;
};

/*
 * Parameters used when a buy signal is actually filled.
 *
 * take_profit_fraction and stop_loss_fraction are decimal percentages of the
 * entry price.  For example, 0.05 means five percent.  A value of exactly
 * 0.0 disables that protective exit.  This is intentional so an Optuna trial
 * that returns zero means "no take profit" or "no stop loss", rather than a
 * target at the entry price.
 */
struct RiskParameters {
  double cash_allocation_fraction = 1.0;
  double take_profit_fraction = 0.0;
  double stop_loss_fraction = 0.0;
};

/* A complete parameter set selected for one walk-forward period. */
struct ParameterSet {
  MovingAverageParameters moving_average;
  RiskParameters risk;
};

/*
 * Makes a new ParameterSet active for signals generated after the candle
 * whose close timestamp is at or after effective_close_timestamp.  Existing
 * trades keep the price levels calculated at their own entry; a parameter
 * swap never silently changes their risk settings or liquidates them.
 */
struct ParameterChange {
  Timestamp effective_close_timestamp = 0;
  ParameterSet parameters;
};

/*
 * Inclusive candle-close bounds for the part of a dataset that contributes to
 * the result.  Earlier supplied candles are warm-up data only: they give a
 * strategy enough history but create no orders or equity observations.
 */
struct BacktestWindow {
  Timestamp start_close_timestamp_unix_seconds = 0;
  Timestamp end_close_timestamp_unix_seconds = 0;
};

}  // namespace trading_optimizer
