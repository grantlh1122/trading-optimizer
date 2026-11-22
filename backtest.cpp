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

   int candle_index = period_start - warmup_period;
   loadWarmupCandles(warmup_period, candle_index, *strategy);
   //trades on [period_start, period_end)
   for(; candle_index < period_end; ++candle_index){
      check_candle_against_orders(account, candles.at(candle_index));
      if(candles[candle_index].timestamp + 4000 < candles[candle_index + 1].timestamp){
         equity_history.push_back(account.get_equity(candles[candle_index].close));
      }

      OrderRequest signal = strategy->generateSignal(candles.at(candle_index));
      double next_open = candles.at(candle_index + 1).open;
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
      
      
   }
   result.final_equity = account.get_equity(calculate_sell_slippage(candles[period_end].open));
   result.trade_count = account.get_trade_count();
   result.win_rate = (account.get_trade_count() == 0) ? 0 : 1.0 * (account.get_win_count() + 
      account.get_num_winning_positions(calculate_sell_slippage(candles[period_end].open)))
      / account.get_trade_count();

   //calculate sharpe ratio. for now, its based on a 6 month window of hour candles

   //NEXT ORDER OF BUSINESS: then use that timestamp to 
   //add 1 candle to equity history every day. pull sharpe into its own function, write 
   //documentation stating that it returns an annualized sharpe ratio, based on the number
   //of days in the equity history. then we are ready to finally write unit tests for strategy
   //and backtester.

   result.sharpe = calculate_sharpe(equity_history);

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
   int& candle_index, Strategy& strategy){
   for(int i = 0; i < warmup_period; ++i){
      strategy.loadCandle(candles.at(candle_index));
      ++candle_index;
   }
}

double Backtest::calculate_sell_slippage(double price){
  return price - price * SLIPPAGE_RATE;
}

double Backtest::calculate_sharpe(const std::vector<double>& equity_history){

}