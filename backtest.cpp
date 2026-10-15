/*
 * Accounting primitives for a one-ticker, hourly-candle backtester.
 *
 * This file deliberately provides no strategy logic and performs no file,
 * database, or Python input. A future caller will decide when to call buy()
 * and sell(), based on a strategy implemented elsewhere. Likewise, a future
 * data layer will feed each completed one-hour candle into
 * record_hourly_equity().
 *
 * What this file owns
 * -------------------
 * - The cash account, initialized to $1,000,000 for a new backtest.
 * - Open trades, including their entry price, remaining share quantity, and
 *   stored take-profit / stop-loss settings.
 * - Mark-to-market equity at each supplied hourly close.
 * - Sharpe ratio calculated from the hourly equity-return series.
 *
 * What this file does not own yet
 * -------------------------------
 * - Fetching or parsing candles.
 * - Deciding whether an order should be opened or closed.
 * - Automatically applying take profit or stop loss levels.
 * - Commissions, slippage, margin, short sales, splits, dividends, or taxes.
 * - Python bindings or a C++ strategy file.
 *
 * Walk-forward design
 * -------------------
 * BacktesterState contains the account state that must survive a parameter
 * change: cash, open trades, the next trade id, and the hourly equity curve.
 * A future Python wrapper can take snapshot_state() at a six-month boundary,
 * start another Backtester with that state, and give the new strategy its new
 * parameters. Strategy parameters are intentionally not stored here, so
 * changing them cannot modify an already-open trade. No position is sold
 * merely because a backtest segment or final run ends.
 */

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace trading_optimizer {

// All cash, prices, and notional values in this file use U.S. dollars.
constexpr double kInitialCashDollars = 1'000'000.0;

// Trade id zero is reserved to mean that a buy request was not accepted.
using TradeId = std::uint64_t;
constexpr TradeId kNoTradeId = 0;

/*
 * An order request passed into the accounting layer.
 *
 * requested_notional_dollars is the dollar amount the future strategy wanted
 * to allocate. quantity is the resulting whole-share amount chosen by that
 * caller. Keeping both values makes the sizing decision visible: the
 * backtester does not silently convert dollars to shares or choose a rounding
 * rule. The actual cash removed is execution_price_dollars * quantity, because
 * that is the amount the simulated fill costs.
 *
 * A target value of 0.0 means that the corresponding take profit or stop loss
 * is not set. These values are stored with the trade only. This skeleton does
 * not inspect a candle or trigger an exit from either level.
 */
struct Order {
  double take_profit_price_dollars = 0.0;
  double stop_loss_price_dollars = 0.0;
  double requested_notional_dollars = 0.0;
  int quantity = 0;
};

/*
 * One currently open trade. An unordered map keyed by trade_id stores these
 * objects, allowing a later strategy to find a specific trade for a partial or
 * full sale without scanning every active trade.
 */
struct OpenTrade {
  TradeId trade_id = kNoTradeId;
  Order order;
  double entry_price_dollars = 0.0;
  double entry_notional_dollars = 0.0;
  int remaining_quantity = 0;
};

/*
 * A mark-to-market account value after one candle has closed. The timestamp is
 * Unix time in seconds at the close of that one-hour candle. The backtester
 * only requires timestamps to be strictly increasing; it does not fetch or
 * validate an hourly market calendar.
 */
struct HourlyEquityPoint {
  std::int64_t close_timestamp_unix_seconds = 0;
  double equity_dollars = 0.0;
};

/*
 * The state that can cross a walk-forward segment boundary.
 *
 * Carrying hourly_equity_curve forward is essential: final Sharpe ratio uses
 * returns across the whole final run, including time before a parameter swap.
 * The order map also means an open trade remains open after a new Backtester
 * starts. The next id is carried forward so trade ids remain unique.
 */
struct BacktesterState {
  double cash_dollars = kInitialCashDollars;
  std::unordered_map<TradeId, OpenTrade> open_trades;
  TradeId next_trade_id = 1;
  std::vector<HourlyEquityPoint> hourly_equity_curve;
};

/*
 * buy() can be rejected without throwing when the account lacks cash. A zero
 * trade id is always a rejected request. Invalid values such as a negative
 * price are programming errors and throw std::invalid_argument instead.
 */
struct BuyResult {
  bool accepted = false;
  TradeId trade_id = kNoTradeId;
  double cash_required_dollars = 0.0;
};

/*
 * Sharpe ratio is undefined without enough return observations, when account
 * equity is non-positive, or when all returns have zero variation. The status
 * lets the result layer explain that condition instead of treating it as a
 * performance score of zero.
 */
enum class SharpeStatus {
  kAvailable,
  kNotEnoughHourlyReturns,
  kNonPositiveEquity,
  kZeroReturnVolatility,
};

struct SharpeRatioResult {
  SharpeStatus status = SharpeStatus::kNotEnoughHourlyReturns;
  double value = 0.0;
  std::size_t hourly_return_count = 0;
};

class Backtester {
 public:
  /*
   * Starts a new single-ticker account with $1,000,000 in cash and no open
   * trades. ticker_symbol identifies the ticker whose hourly close price will
   * be supplied to record_hourly_equity(); it does not cause data to load.
   */
  explicit Backtester(std::string ticker_symbol)
      : ticker_symbol_(std::move(ticker_symbol)) {
    validate_ticker_symbol();
  }

  /*
   * Restores a previously captured account for the next walk-forward segment.
   * The incoming state must be from the same ticker and use the same currency.
   * Those higher-level checks belong to the future wrapper because this state
   * intentionally stores no market-data source or strategy parameters.
   */
  Backtester(std::string ticker_symbol, BacktesterState restored_state)
      : ticker_symbol_(std::move(ticker_symbol)),
        state_(std::move(restored_state)) {
    validate_ticker_symbol();
    validate_restored_state();
  }

  /*
   * Opens a trade at a price already selected by the future execution model.
   * No signal, candle lookup, transaction fee, slippage, or target/stop check
   * happens here. The request is rejected only if the account cannot pay the
   * fill cost with its current cash balance.
   */
  BuyResult buy(const Order& order, double execution_price_dollars) {
    validate_order(order);
    validate_positive_finite(execution_price_dollars, "execution price");

    const double fill_cost =
        execution_price_dollars * static_cast<double>(order.quantity);
    if (fill_cost > state_.cash_dollars) {
      return BuyResult{false, kNoTradeId, fill_cost};
    }

    const TradeId trade_id = state_.next_trade_id++;
    state_.cash_dollars -= fill_cost;
    state_.open_trades.emplace(
        trade_id,
        OpenTrade{trade_id, order, execution_price_dollars, fill_cost,
                  order.quantity});

    return BuyResult{true, trade_id, fill_cost};
  }

  /*
   * Sells part or all of one existing trade at a caller-provided fill price.
   * A full sale removes that trade from open_trades. Selling a smaller amount
   * leaves the remaining shares and the original target/stop settings intact.
   * False means the requested trade did not exist or did not contain enough
   * shares; invalid price or quantity values throw std::invalid_argument.
   */
  bool sell(TradeId trade_id, int quantity, double execution_price_dollars) {
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

  /*
   * Returns cash plus the current market value of all still-open shares.
   * This is account equity, not realized cash. It is what lets an open trade
   * contribute to returns without selling it at the end of a segment.
   */
  double current_equity(double close_price_dollars) const {
    validate_positive_finite(close_price_dollars, "close price");

    double open_position_value = 0.0;
    for (const auto& entry : state_.open_trades) {
      const OpenTrade& trade = entry.second;
      open_position_value +=
          close_price_dollars * static_cast<double>(trade.remaining_quantity);
    }
    return state_.cash_dollars + open_position_value;
  }

  /*
   * Records the account value at the close of one supplied hourly candle.
   * Calls must arrive in time order. This does not sell any shares; it only
   * marks them to the candle's close price so hourly portfolio returns can be
   * calculated later.
   */
  void record_hourly_equity(std::int64_t close_timestamp_unix_seconds,
                            double close_price_dollars) {
    if (!state_.hourly_equity_curve.empty() &&
        close_timestamp_unix_seconds <=
            state_.hourly_equity_curve.back().close_timestamp_unix_seconds) {
      throw std::invalid_argument(
          "hourly candle timestamps must be strictly increasing");
    }

    state_.hourly_equity_curve.push_back(
        HourlyEquityPoint{close_timestamp_unix_seconds,
                          current_equity(close_price_dollars)});
  }

  /*
   * Calculates annualized Sharpe ratio from consecutive hourly equity values.
   *
   * Each simple return is (current_equity / prior_equity) - 1. The risk-free
   * rate is converted from an annual rate to one period before it is removed
   * from each hourly return. periods_per_year is an explicit argument because
   * a 24/7 market and a market with limited trading hours need different
   * annualization choices. The future Python layer should pass the convention
   * it uses for the supplied asset.
   *
   * The function needs at least two hourly returns (three equity points) to
   * calculate a sample standard deviation. It never closes positions.
   */
  SharpeRatioResult calculate_sharpe_ratio(
      double periods_per_year, double annual_risk_free_rate = 0.0) const {
    validate_positive_finite(periods_per_year, "periods per year");
    if (!std::isfinite(annual_risk_free_rate) || annual_risk_free_rate <= -1.0) {
      throw std::invalid_argument(
          "annual risk-free rate must be finite and greater than -1");
    }

    const std::size_t equity_count = state_.hourly_equity_curve.size();
    if (equity_count < 3) {
      return SharpeRatioResult{SharpeStatus::kNotEnoughHourlyReturns, 0.0,
                               equity_count == 0 ? 0 : equity_count - 1};
    }

    const double risk_free_per_period =
        std::pow(1.0 + annual_risk_free_rate, 1.0 / periods_per_year) - 1.0;
    std::vector<double> excess_returns;
    excess_returns.reserve(equity_count - 1);

    for (std::size_t i = 1; i < equity_count; ++i) {
      const double previous_equity =
          state_.hourly_equity_curve[i - 1].equity_dollars;
      const double current_equity_value =
          state_.hourly_equity_curve[i].equity_dollars;
      if (previous_equity <= 0.0 || current_equity_value <= 0.0) {
        return SharpeRatioResult{SharpeStatus::kNonPositiveEquity, 0.0,
                                 excess_returns.size()};
      }

      const double simple_return = current_equity_value / previous_equity - 1.0;
      excess_returns.push_back(simple_return - risk_free_per_period);
    }

    double mean_excess_return = 0.0;
    for (const double excess_return : excess_returns) {
      mean_excess_return += excess_return;
    }
    mean_excess_return /= static_cast<double>(excess_returns.size());

    double squared_deviation_sum = 0.0;
    for (const double excess_return : excess_returns) {
      const double deviation = excess_return - mean_excess_return;
      squared_deviation_sum += deviation * deviation;
    }

    const double sample_variance =
        squared_deviation_sum /
        static_cast<double>(excess_returns.size() - 1);
    const double sample_standard_deviation = std::sqrt(sample_variance);
    if (sample_standard_deviation == 0.0) {
      return SharpeRatioResult{SharpeStatus::kZeroReturnVolatility, 0.0,
                               excess_returns.size()};
    }

    const double annualized_sharpe =
        std::sqrt(periods_per_year) * mean_excess_return /
        sample_standard_deviation;
    return SharpeRatioResult{SharpeStatus::kAvailable, annualized_sharpe,
                             excess_returns.size()};
  }

  /*
   * Produces a copy suitable for the next walk-forward segment. It includes
   * only accounting history and active trades, never strategy parameters or a
   * decision to close positions.
   */
  BacktesterState snapshot_state() const { return state_; }

 private:
  static void validate_positive_finite(double value, const char* field_name) {
    if (!std::isfinite(value) || value <= 0.0) {
      throw std::invalid_argument(std::string(field_name) +
                                  " must be finite and greater than zero");
    }
  }

  static void validate_order(const Order& order) {
    validate_positive_finite(order.requested_notional_dollars,
                             "requested notional");
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

  void validate_ticker_symbol() const {
    if (ticker_symbol_.empty()) {
      throw std::invalid_argument("ticker symbol must not be empty");
    }
  }

  void validate_restored_state() const {
    if (!std::isfinite(state_.cash_dollars) || state_.cash_dollars < 0.0) {
      throw std::invalid_argument("restored cash must be finite and non-negative");
    }
    if (state_.next_trade_id == kNoTradeId) {
      throw std::invalid_argument("restored next trade id must not be zero");
    }
  }

  // This version values all open trades with one supplied ticker close price.
  std::string ticker_symbol_;
  BacktesterState state_;
};

}  // namespace trading_optimizer
