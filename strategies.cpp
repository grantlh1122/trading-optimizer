/*
 * Concrete strategy implementations.
 *
 * This file intentionally contains no cash, order, fill, or market-calendar
 * code.  Those concerns belong to Backtester so every strategy uses identical
 * execution rules and can be compared fairly.
 */

#include "strategies.hpp"

#include <cstddef>
#include <stdexcept>
#include <vector>

namespace trading_optimizer {
namespace {

double simple_moving_average(const std::vector<Candle>& candles,
                             std::size_t end_exclusive,
                             std::size_t window_size) {
  double close_sum = 0.0;
  const std::size_t first_index = end_exclusive - window_size;
  for (std::size_t index = first_index; index < end_exclusive; ++index) {
    close_sum += candles[index].close_price_dollars;
  }
  return close_sum / static_cast<double>(window_size);
}

void validate_moving_average_parameters(
    const MovingAverageParameters& parameters) {
  if (parameters.short_window_candles == 0 ||
      parameters.long_window_candles == 0) {
    throw std::invalid_argument("moving-average windows must be greater than zero");
  }
  if (parameters.short_window_candles >= parameters.long_window_candles) {
    throw std::invalid_argument(
        "short moving-average window must be smaller than long window");
  }
}

}  // namespace

StrategySignal MovingAverageCrossoverStrategy::generate_signal(
    const std::vector<Candle>& historical_candles,
    const ParameterSet& parameters) const {
  const MovingAverageParameters& moving_average = parameters.moving_average;
  validate_moving_average_parameters(moving_average);

  /*
   * Two long-window averages are needed: one ending at the current close and
   * one ending at the preceding close.  Returning Hold during warm-up avoids
   * treating an already-existing trend as a fresh crossover.
   */
  if (historical_candles.size() < moving_average.long_window_candles + 1) {
    return StrategySignal{SignalAction::kHold};
  }

  const std::size_t current_end = historical_candles.size();
  const std::size_t previous_end = current_end - 1;

  const double current_short = simple_moving_average(
      historical_candles, current_end, moving_average.short_window_candles);
  const double current_long = simple_moving_average(
      historical_candles, current_end, moving_average.long_window_candles);
  const double previous_short = simple_moving_average(
      historical_candles, previous_end, moving_average.short_window_candles);
  const double previous_long = simple_moving_average(
      historical_candles, previous_end, moving_average.long_window_candles);

  if (previous_short <= previous_long && current_short > current_long) {
    return StrategySignal{SignalAction::kBuy};
  }
  if (previous_short >= previous_long && current_short < current_long) {
    return StrategySignal{SignalAction::kSell};
  }
  return StrategySignal{SignalAction::kHold};
}

}  // namespace trading_optimizer
