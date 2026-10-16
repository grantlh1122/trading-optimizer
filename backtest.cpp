/*
 * The execution and accounting half of the trading engine.
 *
 * strategies.cpp decides only whether a completed candle implies Buy, Sell,
 * or Hold.  This file deliberately owns all timing-sensitive behavior: orders
 * are queued at a signal close and filled at a later market open, risk exits
 * are evaluated from the bar after a fill is possible, and account value is
 * marked at every evaluated close.
 */

#include "backtest.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace trading_optimizer {

Backtester::Backtester(std::string ticker_symbol)
    : ticker_symbol_(std::move(ticker_symbol)) {
  validate_ticker_symbol();
}

Backtester::Backtester(std::string ticker_symbol, BacktesterState restored_state)
    : ticker_symbol_(std::move(ticker_symbol)),
      state_(std::move(restored_state)) {
  validate_ticker_symbol();
  validate_restored_state();
}

BuyResult Backtester::buy(const Order& order, double execution_price_dollars) {
  validate_order(order);
  validate_positive_finite(execution_price_dollars, "execution price");

  const double fill_cost =
      execution_price_dollars * static_cast<double>(order.quantity);
  if (fill_cost > state_.cash_dollars) {
    return BuyResult{false, kNoTradeId, fill_cost};
  }
  if (state_.next_trade_id == std::numeric_limits<TradeId>::max()) {
    throw std::overflow_error("trade id space is exhausted");
  }

  const TradeId trade_id = state_.next_trade_id++;
  state_.cash_dollars -= fill_cost;
  state_.open_trades.emplace(
      trade_id,
      OpenTrade{trade_id, order, execution_price_dollars, fill_cost,
                order.quantity});
  return BuyResult{true, trade_id, fill_cost};
}

bool Backtester::sell(TradeId trade_id, int quantity,
                      double execution_price_dollars) {
  if (quantity <= 0) {
    throw std::invalid_argument("sell quantity must be greater than zero");
  }
  validate_positive_finite(execution_price_dollars, "execution price");

  const auto trade_it = state_.open_trades.find(trade_id);
  if (trade_it == state_.open_trades.end() ||
      quantity > trade_it->second.remaining_quantity) {
    return false;
  }

  state_.cash_dollars +=
      execution_price_dollars * static_cast<double>(quantity);
  trade_it->second.remaining_quantity -= quantity;
  if (trade_it->second.remaining_quantity == 0) {
    state_.open_trades.erase(trade_it);
  }
  return true;
}

double Backtester::current_equity(double close_price_dollars) const {
  validate_positive_finite(close_price_dollars, "close price");

  double open_position_value = 0.0;
  for (const auto& entry : state_.open_trades) {
    const OpenTrade& trade = entry.second;
    open_position_value +=
        close_price_dollars * static_cast<double>(trade.remaining_quantity);
  }
  return state_.cash_dollars + open_position_value;
}

void Backtester::record_equity(Timestamp close_timestamp_unix_seconds,
                               double close_price_dollars) {
  if (!state_.equity_curve.empty() &&
      close_timestamp_unix_seconds <=
          state_.equity_curve.back().close_timestamp_unix_seconds) {
    throw std::invalid_argument(
        "equity timestamps must be strictly increasing");
  }
  state_.equity_curve.push_back(EquityPoint{close_timestamp_unix_seconds,
                                             current_equity(close_price_dollars)});
}

SharpeRatioResult Backtester::calculate_sharpe_ratio(
    double periods_per_year, double annual_risk_free_rate) const {
  validate_positive_finite(periods_per_year, "periods per year");
  if (!std::isfinite(annual_risk_free_rate) || annual_risk_free_rate <= -1.0) {
    throw std::invalid_argument(
        "annual risk-free rate must be finite and greater than -1");
  }

  const std::size_t equity_count = state_.equity_curve.size();
  if (equity_count < 3) {
    return SharpeRatioResult{SharpeStatus::kNotEnoughReturns, 0.0,
                             equity_count == 0 ? 0 : equity_count - 1};
  }

  const double risk_free_per_period =
      std::pow(1.0 + annual_risk_free_rate, 1.0 / periods_per_year) - 1.0;
  std::vector<double> excess_returns;
  excess_returns.reserve(equity_count - 1);

  for (std::size_t index = 1; index < equity_count; ++index) {
    const double previous_equity = state_.equity_curve[index - 1].equity_dollars;
    const double current_equity_value = state_.equity_curve[index].equity_dollars;
    if (previous_equity <= 0.0 || current_equity_value <= 0.0) {
      return SharpeRatioResult{SharpeStatus::kNonPositiveEquity, 0.0,
                               excess_returns.size()};
    }
    excess_returns.push_back(current_equity_value / previous_equity - 1.0 -
                             risk_free_per_period);
  }

  double mean_excess_return = 0.0;
  for (const double return_value : excess_returns) {
    mean_excess_return += return_value;
  }
  mean_excess_return /= static_cast<double>(excess_returns.size());

  double squared_deviation_sum = 0.0;
  for (const double return_value : excess_returns) {
    const double deviation = return_value - mean_excess_return;
    squared_deviation_sum += deviation * deviation;
  }
  const double sample_variance =
      squared_deviation_sum / static_cast<double>(excess_returns.size() - 1);
  const double sample_standard_deviation = std::sqrt(sample_variance);
  if (sample_standard_deviation == 0.0) {
    return SharpeRatioResult{SharpeStatus::kZeroReturnVolatility, 0.0,
                             excess_returns.size()};
  }

  return SharpeRatioResult{
      SharpeStatus::kAvailable,
      std::sqrt(periods_per_year) * mean_excess_return / sample_standard_deviation,
      excess_returns.size()};
}

BacktesterState Backtester::snapshot_state() const { return state_; }

BacktestRunResult Backtester::run(
    const std::vector<Candle>& downloaded_candles, const BacktestWindow& window,
    const std::vector<ParameterChange>& parameter_schedule,
    const Strategy& strategy) {
  if (window.start_close_timestamp_unix_seconds >
      window.end_close_timestamp_unix_seconds) {
    throw std::invalid_argument("backtest window start must not be after its end");
  }
  if (parameter_schedule.empty()) {
    throw std::invalid_argument("parameter schedule must contain an initial set");
  }
  if (parameter_schedule.front().effective_close_timestamp >
      window.start_close_timestamp_unix_seconds) {
    throw std::invalid_argument(
        "the first parameter set must be effective on or before the window start");
  }

  for (std::size_t index = 0; index < parameter_schedule.size(); ++index) {
    validate_parameter_set(parameter_schedule[index].parameters);
    if (index > 0 &&
        parameter_schedule[index].effective_close_timestamp <=
            parameter_schedule[index - 1].effective_close_timestamp) {
      throw std::invalid_argument(
          "parameter changes must have strictly increasing effective timestamps");
    }
  }

  std::size_t active_parameter_index = 0;
  Timestamp previous_download_timestamp =
      state_.candle_history.empty()
          ? std::numeric_limits<Timestamp>::min()
          : state_.candle_history.back().close_timestamp_unix_seconds;
  std::size_t evaluated_candle_count = 0;
  Timestamp last_evaluated_close_timestamp = 0;

  for (const Candle& candle : downloaded_candles) {
    validate_candle(candle);
    if (candle.close_timestamp_unix_seconds <= previous_download_timestamp) {
      throw std::invalid_argument(
          "downloaded candle timestamps must be strictly increasing");
    }
    previous_download_timestamp = candle.close_timestamp_unix_seconds;

    if (candle.close_timestamp_unix_seconds <
        window.start_close_timestamp_unix_seconds) {
      ingest_warmup_candle(candle);
      continue;
    }
    if (candle.close_timestamp_unix_seconds >
        window.end_close_timestamp_unix_seconds) {
      break;
    }

    while (active_parameter_index + 1 < parameter_schedule.size() &&
           parameter_schedule[active_parameter_index + 1]
                   .effective_close_timestamp <=
               candle.close_timestamp_unix_seconds) {
      ++active_parameter_index;
    }
    process_candle(candle, parameter_schedule[active_parameter_index].parameters,
                   strategy);
    ++evaluated_candle_count;
    last_evaluated_close_timestamp = candle.close_timestamp_unix_seconds;
  }

  if (evaluated_candle_count == 0) {
    throw std::invalid_argument("no downloaded candles fall inside the backtest window");
  }

  return BacktestRunResult{snapshot_state(), evaluated_candle_count,
                           active_parameter_index + 1,
                           last_evaluated_close_timestamp};
}

void Backtester::ingest_warmup_candle(const Candle& candle) {
  validate_candle(candle);
  validate_next_candle_time(candle);
  state_.candle_history.push_back(candle);
}

void Backtester::process_candle(const Candle& candle,
                                const ParameterSet& parameters,
                                const Strategy& strategy) {
  validate_candle(candle);
  validate_parameter_set(parameters);
  validate_next_candle_time(candle);

  if (candle.market_status == MarketStatus::kOpen) {
    execute_pending_orders_at_open(candle);
    execute_protective_exits(candle);
  }

  /* Loading exactly this one completed candle is what advances strategy time. */
  state_.candle_history.push_back(candle);
  record_equity(candle.close_timestamp_unix_seconds, candle.close_price_dollars);
  queue_signal(strategy.generate_signal(state_.candle_history, parameters), candle,
               parameters);
}

void Backtester::execute_pending_orders_at_open(const Candle& candle) {
  if (candle.market_status != MarketStatus::kOpen) {
    return;
  }

  std::vector<PendingOrder> pending_orders = std::move(state_.pending_orders);
  state_.pending_orders.clear();

  for (const PendingOrder& pending_order : pending_orders) {
    if (pending_order.action == SignalAction::kBuy) {
      /* This is a long-only engine: one Buy signal opens one aggregate position. */
      if (has_open_position()) {
        continue;
      }

      const double requested_notional =
          state_.cash_dollars *
          pending_order.parameters_at_signal.risk.cash_allocation_fraction;
      const double whole_share_quantity =
          std::floor(requested_notional / candle.open_price_dollars);
      if (whole_share_quantity < 1.0) {
        /* Insufficient cash for a whole share is a rejected simulated order. */
        continue;
      }
      if (whole_share_quantity >
          static_cast<double>(std::numeric_limits<int>::max())) {
        throw std::overflow_error("buy quantity exceeds supported integer range");
      }

      const RiskParameters& risk = pending_order.parameters_at_signal.risk;
      const Order order{
          risk.take_profit_fraction == 0.0
              ? 0.0
              : candle.open_price_dollars * (1.0 + risk.take_profit_fraction),
          risk.stop_loss_fraction == 0.0
              ? 0.0
              : candle.open_price_dollars * (1.0 - risk.stop_loss_fraction),
          requested_notional, static_cast<int>(whole_share_quantity)};
      const BuyResult result = buy(order, candle.open_price_dollars);
      if (result.accepted) {
        append_fill(candle.open_timestamp_unix_seconds, SignalAction::kBuy,
                    FillReason::kStrategySignal, result.trade_id, order.quantity,
                    candle.open_price_dollars);
      }
      continue;
    }

    if (pending_order.action == SignalAction::kSell) {
      std::vector<TradeId> trade_ids;
      trade_ids.reserve(state_.open_trades.size());
      for (const auto& entry : state_.open_trades) {
        trade_ids.push_back(entry.first);
      }
      for (const TradeId trade_id : trade_ids) {
        const auto trade_it = state_.open_trades.find(trade_id);
        if (trade_it == state_.open_trades.end()) {
          continue;
        }
        const int quantity = trade_it->second.remaining_quantity;
        if (sell(trade_id, quantity, candle.open_price_dollars)) {
          append_fill(candle.open_timestamp_unix_seconds, SignalAction::kSell,
                      FillReason::kStrategySignal, trade_id, quantity,
                      candle.open_price_dollars);
        }
      }
    }
  }
}

void Backtester::execute_protective_exits(const Candle& candle) {
  std::vector<TradeId> trade_ids;
  trade_ids.reserve(state_.open_trades.size());
  for (const auto& entry : state_.open_trades) {
    trade_ids.push_back(entry.first);
  }

  for (const TradeId trade_id : trade_ids) {
    const auto trade_it = state_.open_trades.find(trade_id);
    if (trade_it == state_.open_trades.end()) {
      continue;
    }
    const OpenTrade& trade = trade_it->second;
    const double stop_price = trade.order.stop_loss_price_dollars;
    const double target_price = trade.order.take_profit_price_dollars;
    double fill_price = 0.0;
    FillReason reason = FillReason::kStrategySignal;

    /*
     * Gap fills use the open, because a stop/target price was unavailable.
     * When both levels lie inside one OHLC bar, bar data cannot tell which was
     * hit first.  The stop is intentionally chosen first as the conservative
     * long-position assumption.
     */
    if (stop_price > 0.0 && candle.open_price_dollars <= stop_price) {
      fill_price = candle.open_price_dollars;
      reason = FillReason::kStopLoss;
    } else if (target_price > 0.0 && candle.open_price_dollars >= target_price) {
      fill_price = candle.open_price_dollars;
      reason = FillReason::kTakeProfit;
    } else if (stop_price > 0.0 && candle.low_price_dollars <= stop_price) {
      fill_price = stop_price;
      reason = FillReason::kStopLoss;
    } else if (target_price > 0.0 && candle.high_price_dollars >= target_price) {
      fill_price = target_price;
      reason = FillReason::kTakeProfit;
    }

    if (fill_price > 0.0) {
      const int quantity = trade.remaining_quantity;
      if (sell(trade_id, quantity, fill_price)) {
        append_fill(candle.open_timestamp_unix_seconds, SignalAction::kSell,
                    reason, trade_id, quantity, fill_price);
      }
    }
  }
}

void Backtester::queue_signal(const StrategySignal& signal, const Candle& candle,
                              const ParameterSet& parameters) {
  if (signal.action == SignalAction::kHold) {
    return;
  }

  if (signal.action == SignalAction::kBuy) {
    /* A newer Buy cancels a queued Sell; the latest close decision wins. */
    state_.pending_orders.erase(
        std::remove_if(state_.pending_orders.begin(), state_.pending_orders.end(),
                       [](const PendingOrder& order) {
                         return order.action == SignalAction::kSell;
                       }),
        state_.pending_orders.end());
    if (!has_open_position() && !has_pending_action(SignalAction::kBuy)) {
      state_.pending_orders.push_back(PendingOrder{
          SignalAction::kBuy, candle.close_timestamp_unix_seconds, parameters});
    }
    return;
  }

  if (signal.action == SignalAction::kSell) {
    /* A sell while flat cancels a queued buy rather than creating a short. */
    state_.pending_orders.erase(
        std::remove_if(state_.pending_orders.begin(), state_.pending_orders.end(),
                       [](const PendingOrder& order) {
                         return order.action == SignalAction::kBuy;
                       }),
        state_.pending_orders.end());
    if (has_open_position() && !has_pending_action(SignalAction::kSell)) {
      state_.pending_orders.push_back(PendingOrder{
          SignalAction::kSell, candle.close_timestamp_unix_seconds, parameters});
    }
  }
}

bool Backtester::has_open_position() const { return !state_.open_trades.empty(); }

bool Backtester::has_pending_action(SignalAction action) const {
  return std::any_of(state_.pending_orders.begin(), state_.pending_orders.end(),
                     [action](const PendingOrder& order) {
                       return order.action == action;
                     });
}

void Backtester::validate_candle(const Candle& candle) {
  if (candle.open_timestamp_unix_seconds >= candle.close_timestamp_unix_seconds) {
    throw std::invalid_argument("candle open timestamp must be before close timestamp");
  }
  validate_positive_finite(candle.open_price_dollars, "candle open price");
  validate_positive_finite(candle.high_price_dollars, "candle high price");
  validate_positive_finite(candle.low_price_dollars, "candle low price");
  validate_positive_finite(candle.close_price_dollars, "candle close price");
  if (candle.high_price_dollars < candle.open_price_dollars ||
      candle.high_price_dollars < candle.close_price_dollars ||
      candle.low_price_dollars > candle.open_price_dollars ||
      candle.low_price_dollars > candle.close_price_dollars ||
      candle.low_price_dollars > candle.high_price_dollars) {
    throw std::invalid_argument("candle OHLC prices are inconsistent");
  }
}

void Backtester::validate_parameter_set(const ParameterSet& parameters) {
  const MovingAverageParameters& moving_average = parameters.moving_average;
  if (moving_average.short_window_candles == 0 ||
      moving_average.long_window_candles == 0 ||
      moving_average.short_window_candles >= moving_average.long_window_candles) {
    throw std::invalid_argument(
        "moving-average windows must be positive and short must be smaller than long");
  }

  const RiskParameters& risk = parameters.risk;
  if (!std::isfinite(risk.cash_allocation_fraction) ||
      risk.cash_allocation_fraction <= 0.0 ||
      risk.cash_allocation_fraction > 1.0) {
    throw std::invalid_argument(
        "cash allocation fraction must be finite and in the interval (0, 1]");
  }
  if (!std::isfinite(risk.take_profit_fraction) ||
      risk.take_profit_fraction < 0.0) {
    throw std::invalid_argument(
        "take-profit fraction must be a finite non-negative number");
  }
  if (!std::isfinite(risk.stop_loss_fraction) ||
      risk.stop_loss_fraction < 0.0 || risk.stop_loss_fraction >= 1.0) {
    throw std::invalid_argument(
        "stop-loss fraction must be finite and in the interval [0, 1)");
  }
}

void Backtester::validate_positive_finite(double value, const char* field_name) {
  if (!std::isfinite(value) || value <= 0.0) {
    throw std::invalid_argument(std::string(field_name) +
                                " must be finite and greater than zero");
  }
}

void Backtester::validate_order(const Order& order) {
  validate_positive_finite(order.requested_notional_dollars, "requested notional");
  if (order.quantity <= 0) {
    throw std::invalid_argument("buy quantity must be greater than zero");
  }
  if (!std::isfinite(order.take_profit_price_dollars) ||
      order.take_profit_price_dollars < 0.0) {
    throw std::invalid_argument(
        "take-profit price must be zero or a positive finite number");
  }
  if (!std::isfinite(order.stop_loss_price_dollars) ||
      order.stop_loss_price_dollars < 0.0) {
    throw std::invalid_argument(
        "stop-loss price must be zero or a positive finite number");
  }
}

void Backtester::validate_ticker_symbol() const {
  if (ticker_symbol_.empty()) {
    throw std::invalid_argument("ticker symbol must not be empty");
  }
}

void Backtester::validate_restored_state() const {
  if (!std::isfinite(state_.cash_dollars) || state_.cash_dollars < 0.0) {
    throw std::invalid_argument("restored cash must be finite and non-negative");
  }
  if (state_.next_trade_id == kNoTradeId) {
    throw std::invalid_argument("restored next trade id must not be zero");
  }
  for (const auto& entry : state_.open_trades) {
    const OpenTrade& trade = entry.second;
    if (trade.trade_id == kNoTradeId || trade.remaining_quantity <= 0 ||
        !std::isfinite(trade.entry_price_dollars) ||
        trade.entry_price_dollars <= 0.0 ||
        !std::isfinite(trade.entry_notional_dollars) ||
        trade.entry_notional_dollars <= 0.0) {
      throw std::invalid_argument("restored open trade is invalid");
    }
    validate_order(trade.order);
  }
  for (std::size_t index = 0; index < state_.equity_curve.size(); ++index) {
    const EquityPoint& point = state_.equity_curve[index];
    if (!std::isfinite(point.equity_dollars) || point.equity_dollars <= 0.0 ||
        (index > 0 &&
         point.close_timestamp_unix_seconds <=
             state_.equity_curve[index - 1].close_timestamp_unix_seconds)) {
      throw std::invalid_argument("restored equity curve is invalid");
    }
  }
  for (std::size_t index = 0; index < state_.candle_history.size(); ++index) {
    validate_candle(state_.candle_history[index]);
    if (index > 0 &&
        state_.candle_history[index].close_timestamp_unix_seconds <=
            state_.candle_history[index - 1].close_timestamp_unix_seconds) {
      throw std::invalid_argument("restored candle history is not chronological");
    }
  }
}

void Backtester::validate_next_candle_time(const Candle& candle) const {
  if (!state_.candle_history.empty() &&
      candle.close_timestamp_unix_seconds <=
          state_.candle_history.back().close_timestamp_unix_seconds) {
    throw std::invalid_argument("candle timestamps must be strictly increasing");
  }
}

void Backtester::append_fill(Timestamp candle_open_timestamp,
                             SignalAction action, FillReason reason,
                             TradeId trade_id, int quantity,
                             double execution_price_dollars) {
  state_.fills.push_back(Fill{candle_open_timestamp, action, reason, trade_id,
                              quantity, execution_price_dollars});
}

}  // 