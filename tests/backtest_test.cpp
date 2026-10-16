/* Small dependency-free regression tests for the native event loop. */

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <vector>

#include "backtest.hpp"

namespace {

using trading_optimizer::BacktestWindow;
using trading_optimizer::Backtester;
using trading_optimizer::Candle;
using trading_optimizer::MarketStatus;
using trading_optimizer::MovingAverageCrossoverStrategy;
using trading_optimizer::ParameterChange;
using trading_optimizer::ParameterSet;
using trading_optimizer::SignalAction;
using trading_optimizer::Strategy;
using trading_optimizer::StrategySignal;

void expect(bool condition, const char* message) {
  if (!condition) {
    std::cerr << "test failure: " << message << '\n';
    std::exit(1);
  }
}

Candle make_candle(int bar_number, double open, double high, double low,
                   double close, MarketStatus status = MarketStatus::kOpen) {
  constexpr std::int64_t kSecondsPerHour = 60 * 60;
  return Candle{(bar_number - 1) * kSecondsPerHour,
                bar_number * kSecondsPerHour, open, high, low, close, status};
}

ParameterSet moving_average_parameters() {
  ParameterSet parameters;
  parameters.moving_average.short_window_candles = 2;
  parameters.moving_average.long_window_candles = 3;
  parameters.risk.cash_allocation_fraction = 1.0;
  // These zeros are intentional: the test verifies disabled risk exits.
  parameters.risk.take_profit_fraction = 0.0;
  parameters.risk.stop_loss_fraction = 0.0;
  return parameters;
}

class ScriptedStrategy final : public Strategy {
 public:
  StrategySignal generate_signal(const std::vector<Candle>& history,
                                 const ParameterSet&) const override {
    if (history.size() == 1) {
      return StrategySignal{SignalAction::kBuy};
    }
    if (history.size() == 3) {
      return StrategySignal{SignalAction::kSell};
    }
    return StrategySignal{SignalAction::kHold};
  }
};

void test_moving_average_crossovers() {
  MovingAverageCrossoverStrategy strategy;
  const ParameterSet parameters = moving_average_parameters();
  std::vector<Candle> history;
  const std::vector<double> closes{10.0, 9.0, 8.0, 9.0, 10.0, 9.0, 8.0};

  for (std::size_t index = 0; index < closes.size(); ++index) {
    history.push_back(make_candle(static_cast<int>(index) + 1, closes[index],
                                  closes[index], closes[index], closes[index]));
    const StrategySignal signal = strategy.generate_signal(history, parameters);
    if (index == 4) {
      expect(signal.action == SignalAction::kBuy,
             "an upward moving-average cross must produce Buy");
    }
    if (index == 6) {
      expect(signal.action == SignalAction::kSell,
             "a downward moving-average cross must produce Sell");
    }
  }
}

void test_closed_market_queue_and_disabled_exits() {
  const ParameterSet parameters = moving_average_parameters();
  const std::vector<ParameterChange> schedule{
      ParameterChange{0, parameters},
      // A real walk-forward run can change parameters without restarting the
      // account. Keeping equal settings here isolates the scheduling behavior.
      ParameterChange{6 * 60 * 60, parameters},
  };
  const std::vector<Candle> candles{
      make_candle(1, 10.0, 10.0, 10.0, 10.0),
      make_candle(2, 9.0, 9.0, 9.0, 9.0),
      make_candle(3, 8.0, 8.0, 8.0, 8.0),
      make_candle(4, 9.0, 9.0, 9.0, 9.0),
      // This close produces Buy, but its session is closed.
      make_candle(5, 10.0, 10.0, 10.0, 10.0, MarketStatus::kClosed),
      // There is still no fill while the market is closed.
      make_candle(6, 11.0, 11.0, 11.0, 11.0, MarketStatus::kClosed),
      // The queued order fills at this next tradable open. A very high intrabar
      // high proves a zero take profit does not trigger an exit.
      make_candle(7, 12.0, 100.0, 11.0, 12.0),
  };

  Backtester backtester("TEST");
  MovingAverageCrossoverStrategy strategy;
  const auto result = backtester.run(
      candles, BacktestWindow{60 * 60, 7 * 60 * 60}, schedule, strategy);
  const auto state = result.final_state;

  expect(result.evaluated_candle_count == candles.size(),
         "every in-window candle must be evaluated once");
  expect(result.parameter_sets_applied == 2,
         "the later parameter epoch must be activated in-place");
  expect(state.fills.size() == 1,
         "closed-market candles must not execute a queued buy");
  expect(state.fills.front().action == SignalAction::kBuy,
         "the only fill must be the queued buy");
  expect(std::fabs(state.fills.front().execution_price_dollars - 12.0) < 1e-9,
         "queued buy must fill at the next market-open candle price");
  expect(state.open_trades.size() == 1,
         "zero stop loss and take profit must leave the position open");
  const auto& trade = state.open_trades.begin()->second;
  expect(trade.order.take_profit_price_dollars == 0.0,
         "zero take profit must remain disabled on the order");
  expect(trade.order.stop_loss_price_dollars == 0.0,
         "zero stop loss must remain disabled on the order");
}

void test_closed_market_queues_sell_until_next_open() {
  const ParameterSet parameters = moving_average_parameters();
  const std::vector<Candle> candles{
      make_candle(1, 10.0, 10.0, 10.0, 10.0),
      make_candle(2, 11.0, 11.0, 11.0, 11.0),
      // The scripted strategy creates Sell at this closed candle's close.
      make_candle(3, 10.0, 10.0, 10.0, 10.0, MarketStatus::kClosed),
      make_candle(4, 9.0, 9.0, 9.0, 9.0, MarketStatus::kClosed),
      make_candle(5, 7.0, 7.0, 7.0, 7.0),
  };

  Backtester backtester("TEST");
  ScriptedStrategy strategy;
  const auto result = backtester.run(
      candles, BacktestWindow{60 * 60, 5 * 60 * 60},
      std::vector<ParameterChange>{ParameterChange{0, parameters}}, strategy);

  expect(result.final_state.fills.size() == 2,
         "the test must have exactly one buy and one delayed sell");
  expect(result.final_state.fills[1].action == SignalAction::kSell,
         "the second fill must be the queued sell");
  expect(std::fabs(result.final_state.fills[1].execution_price_dollars - 7.0) <
             1e-9,
         "a closed-market sell must execute at the next market-open price");
  expect(result.final_state.open_trades.empty(),
         "the delayed sell must close the long position");
}

}  // namespace

int main() {
  test_moving_average_crossovers();
  test_closed_market_queue_and_disabled_exits();
  test_closed_market_queues_sell_until_next_open();
  std::cout << "native backtest tests passed\n";
  return 0;
}
