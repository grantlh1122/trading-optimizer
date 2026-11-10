#include "backtest.hpp"
//loop over all candles, add one by one to strategy
//when strategy executes based on close price, place order on next open
//sell logic is tougher: based on high, low. Take profit at the takeprofit IF
//candle high is greater than takeprofit and same for stoploss. However,
//the market can jump over (ie close above stoploss and open below) in this case, fill at open.



Backtest::Backtest() = default;

void Backtest::loadCandles(const std::vector<Candle>& input) {
   if (!candlesLoaded){
      candles = input;
      candlesLoaded = true;
   }
}

BacktestResult Backtest::runBacktest(Account& account, const std::string& strategy_id, 
      const ParameterSet& params, const int period_start, const int period_end, const int warmup_period){

   std::unique_ptr<Strategy> strategy = strategyFactory(strategy_id, params);
   BacktestResult result(strategy_id, params, 
      {period_start, period_end}, account.get_equity(candles[period_start].open));
   account.reset_counters();

   std::vector<double> equity_history;
   equity_history.reserve(period_end - period_start + 1);
   equity_history.push_back(result.initial_equity);

   int next_candle_index = period_start - warmup_period;
   loadWarmupCandles(warmup_period, next_candle_index, *strategy);
   //trades on [period_start, period_end)
   for(; next_candle_index < period_end; ++next_candle_index){
      check_candle_against_orders(account, candles.at(next_candle_index));

      OrderRequest signal = strategy->generateSignal(candles.at(next_candle_index));
      double next_open = candles.at(next_candle_index + 1).open;
      double buy_fill = next_open * (1 + SLIPPAGE_RATE);
      double sell_fill = next_open * (1 - SLIPPAGE_RATE);

      if(signal.type == BUY){
         double stop_loss_price = buy_fill * (1 - signal.stop_loss_frac);
         double take_profit_price = buy_fill * (1 + signal.take_profit_frac);
         account.buyOrder(signal.risk_frac, buy_fill, 
            stop_loss_price, take_profit_price);
      } else if(signal.type == SELL){
         account.full_sell_order(sell_fill);
      }
      equity_history.push_back(account.get_equity(next_open));
   }
   result.final_equity = account.get_equity(calculate_sell_slippage(candles[period_end].open));
   result.trade_count = account.get_trade_count();
   result.win_rate = (account.get_trade_count() == 0) ? 0 : 1.0 * (account.get_win_count() + 
      account.get_num_winning_positions(calculate_sell_slippage(candles[period_end].open)))
      / account.get_trade_count();

   //calculate sharpe ratio. for now, its based on a 6 month window of hour candles
   const int n = equity_history.size();
   //const double risk_free_per_candle = std::pow(1 + RISK_FREE_RATE, 0.5 / n) - 1;
   const double risk_free_per_candle = std::pow(1 + RISK_FREE_RATE, 0.25 / (n-1)) - 1;
   std::vector<double> excess_returns;
   excess_returns.reserve(n + 1);
   double sum = 0;

   for(int i = 1; i < equity_history.size(); ++i){
      const double candle_return = equity_history[i] / equity_history[i-1] - 1;
      const double excess_return = candle_return - risk_free_per_candle;
      excess_returns.push_back(excess_return);
      sum += excess_return;
   }

   const double mean = sum / excess_returns.size();
   double squared_devs = 0;
   for(double x : excess_returns){
      squared_devs += (x - mean) * (x - mean);
   }
   const double stdev = std::sqrt(squared_devs / (excess_returns.size()-1));

   result.sharpe = mean/stdev;

   return result;
}

void Backtest::check_candle_against_orders(Account& account, 
   const Candle& candle){
      const std::vector<Position>& positions = account.get_positions();
      std::vector<CloseRequest> close_requests;
      for(int i = 0; i < positions.size(); ++i){
         const Position& current_position = positions.at(i);
         double stop_loss_price = current_position.stop_loss_price;
         double take_profit_price = current_position.take_profit_price;
         
         if(candle.open <= stop_loss_price || candle.open >= take_profit_price){
            double fill_price = calculate_sell_slippage(candle.open);
            close_requests.push_back({current_position.position_id, fill_price});
         } else if(candle.low <= stop_loss_price){
            double fill_price = calculate_sell_slippage(stop_loss_price);
            close_requests.push_back({current_position.position_id, fill_price});
         } else if(candle.high >= take_profit_price){
            double fill_price = calculate_sell_slippage(take_profit_price);
            close_requests.push_back({current_position.position_id, fill_price});
         }
      }
      for(int i = 0; i < close_requests.size(); ++i){
         account.close_position(close_requests.at(i));
      }
   }

   BacktestResult Backtest::runBacktestTraining(const std::string& strategy_id, 
         const ParameterSet& params, const int period_start, const int period_end, const int warmup_period){
      Account account;
      return runBacktest(account, strategy_id, params, period_start, period_end, warmup_period);
   }

void Backtest::loadWarmupCandles(const int warmup_period, 
   int& next_candle_index, Strategy& strategy){
   for(int i = 0; i < warmup_period; ++i){
      strategy.loadCandle(candles.at(next_candle_index));
      ++next_candle_index;
   }
}

double Backtest::calculate_sell_slippage(double price){
  return price - price * SLIPPAGE_RATE;
}