/*
 * Event-driven, long-only accounting and simulation interfaces.
 *
 * Strategies are declared separately in strategies.hpp.  Backtester owns the
 * execution model, trade lifecycle, queued orders, protective exits, equity
 * curve, and walk-forward parameter schedule.
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "strategies.hpp"
#include "trading_types.hpp"

namespace trading_optimizer {

constexpr double kInitialCashDollars = 1'000'000.0;

using TradeId = std::uint64_t;
constexpr TradeId kNoTradeId = 0;

/*
 * Price levels are fixed at entry.  A zero level means that exit is disabled.
 * requested_notional_dollars records the sizing decision before whole-share
 * rounding; quantity is the actual whole-share fill quantity.
 */
struct Order {
  double take_profit_price_dollars = 0.0;
  double stop_loss_price_dollars = 0.0;
  double requested_notional_dollars = 0.0;
  int quantity = 0;
};

struct OpenTrade {
  TradeId trade_id = kNoTradeId;
  Order order;
  double entry_price_dollars = 0.0;
  double entry_notional_dollars = 0.0;
  int remaining_quantity = 0;
};

/* One mark-to-market account value at a completed candle close. */
struct EquityPoint {
  Timestamp close_timestamp_unix_seconds = 0;
  double equity_dollars = 0.0;
};

/* Why a simulated fill happened. */
enum class FillReason { kStrategySignal, kTakeProfit, kStopLoss };

/*
 * A simulated fill.  timestamp identifies the opening time of the candle
 * containing the fill.  A stop/target can happen inside that candle, but bar
 * data cannot reveal an exact intrabar timestamp, so this must not be treated
 * as a tick-level execution time.
 */
struct Fill {
  Timestamp candle_open_timestamp_unix_seconds = 0;
  SignalAction action = SignalAction::kHold;
  FillReason reason = FillReason::kStrategySignal;
  TradeId trade_id = kNoTradeId;
  int quantity = 0;
  double execution_price_dollars = 0.0;
};

/* An unfilled strategy decision waiting for the next tradable candle open. */
struct PendingOrder {
  SignalAction action = SignalAction::kHold;
  Timestamp signal_close_timestamp_unix_seconds = 0;
  ParameterSet parameters_at_signal;
};

/*
 * All state that can cross a walk-forward boundary.  In particular, open
 * trades preserve their original levels, pending orders retain the parameters
 * active when they were signalled, and candle history remains available to a
 * newly selected strategy parameter set.
 */
struct BacktesterState {
  double cash_dollars = kInitialCashDollars;
  std::unordered_map<TradeId, OpenTrade> open_trades;
  TradeId next_trade_id = 1;
  std::vector<EquityPoint> equity_curve;
  std::vector<Candle> candle_history;
  std::vector<PendingOrder> pending_orders;
  std::vector<Fill> fills;
};

struct BuyResult {
  bool accepted = false;
  TradeId trade_id = kNoTradeId;
  double cash_required_dollars = 0.0;
};

enum class SharpeStatus {
  kAvailable,
  kNotEnoughReturns,
  kNonPositiveEquity,
  kZeroReturnVolatility,
};

struct SharpeRatioResult {
  SharpeStatus status = SharpeStatus::kNotEnoughReturns;
  double value = 0.0;
  std::size_t return_count = 0;
};

struct BacktestRunResult {
  BacktesterState final_state;
  std::size_t evaluated_candle_count = 0;
  std::size_t parameter_sets_applied = 0;
  Timestamp last_evaluated_close_timestamp_unix_seconds = 0;
};

class Backtester {
 public:
  explicit Backtester(std::string ticker_symbol);
  Backtester(std::string ticker_symbol, BacktesterState restored_state);

  /*
   * Runs the supplied, already-downloaded candle sequence one candle at a
   * time.  The sequence must be chronological and may include warm-up candles
   * before window.start.  At each in-window candle close, Backtester:
   *
   * 1. fills queued signal orders if this is a market-open candle;
   * 2. checks existing long positions against this candle's stop/target;
   * 3. records close equity;
   * 4. gives the accumulated history and active parameters to strategy; and
   * 5. queues the resulting signal for a later market open.
   *
   * Filling a close-derived signal at a later open avoids using a price that
   * was not available when the decision was made.  A signal generated during
   * a closed session naturally remains queued until the next kOpen candle.
   * Pending orders are not forced at the end of the window.
   */
  BacktestRunResult run(const std::vector<Candle>& downloaded_candles,
                        const BacktestWindow& window,
                        const std::vector<ParameterChange>& parameter_schedule,
                        const Strategy& strategy);

  /* Manual accounting operations are public for focused accounting tests. */
  BuyResult buy(const Order& order, double execution_price_dollars);
  bool sell(TradeId trade_id, int quantity, double execution_price_dollars);

  double current_equity(double close_price_dollars) const;
  void record_equity(Timestamp close_timestamp_unix_seconds,
                     double close_price_dollars);
  SharpeRatioResult calculate_sharpe_ratio(
      double periods_per_year, double annual_risk_free_rate = 0.0) const;

  BacktesterState snapshot_state() const;

 private:
  void ingest_warmup_candle(const Candle& candle);
  void process_candle(const Candle& candle, const ParameterSet& parameters,
                      const Strategy& strategy);
  void execute_pending_orders_at_open(const Candle& candle);
  void execute_protective_exits(const Candle& candle);
  void queue_signal(const StrategySignal& signal, const Candle& candle,
                    const ParameterSet& parameters);
  bool has_open_position() const;
  bool has_pending_action(SignalAction action) const;

  static void validate_candle(const Candle& candle);
  static void validate_parameter_set(const ParameterSet& parameters);
  static void validate_positive_finite(double value, const char* field_name);
  static void validate_order(const Order& order);

  void validate_ticker_symbol() const;
  void validate_restored_state() const;
  void validate_next_candle_time(const Candle& candle) const;
  void append_fill(Timestamp candle_open_timestamp, SignalAction action,
                   FillReason reason, TradeId trade_id, int quantity,
                   double execution_price_dollars);

  std::string ticker_symbol_;
  BacktesterState state_;
};

}  // namespace trading_optimizer
