/* Strategy interfaces and concrete strategy declarations. */

#pragma once

#include <vector>

#include "trading_types.hpp"

namespace trading_optimizer {

/*
 * A strategy is deliberately a pure signal generator.  It receives every
 * completed candle observed so far and the parameter set that is active at
 * that candle's close, then says Buy, Sell, or Hold.  It does not know the
 * account balance, cannot place an order, and cannot see later candles.
 *
 * New strategies should implement this interface in strategies.cpp (or a
 * similarly focused implementation file).  Keeping this boundary narrow
 * makes strategy logic easy to unit-test independently of fills and P&L.
 */
class Strategy {
 public:
  virtual ~Strategy() = default;

  virtual StrategySignal generate_signal(
      const std::vector<Candle>& historical_candles,
      const ParameterSet& parameters) const = 0;
};

/*
 * Long-only moving-average crossover strategy.
 *
 * It buys only when the short moving average crosses from at-or-below the
 * long moving average to above it, and sells on the inverse cross.  It emits
 * Hold until it has enough history to compare both the current and prior
 * moving averages.  The class holds no mutable state; the supplied candle
 * history is the complete source of truth.
 */
class MovingAverageCrossoverStrategy final : public Strategy {
 public:
  StrategySignal generate_signal(
      const std::vector<Candle>& historical_candles,
      const ParameterSet& parameters) const override;
};

}  // namespace trading_optimizer
