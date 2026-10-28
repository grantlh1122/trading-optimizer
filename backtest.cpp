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

Backtest::BacktestResult Backtest::runBacktest(Account& account, const std::string& strategy_id, 
      const ParameterSet& params, const int period_start, const int period_end, const int warmup_period){
   std::unique_ptr<Strategy> strategy = strategyFactory(strategy_id, params);
   int next_candle_index = period_start - warmup_period;
   loadWarmupCandles(warmup_period, next_candle_index, *strategy);
   //trades on [period_start, period_end)
   for(; next_candle_index < period_end; ++next_candle_index){
      check_candle_against_orders(account, candles.at(next_candle_index));

      OrderRequest signal = strategy->generateSignal(candles.at(next_candle_index));
      double next_open = candles.at(next_candle_index + 1).open;
      double next_open_slippage = next_open * SLIPPAGE_RATE;
      
      if(signal.type == BUY){
         double stop_loss_price = next_open * (1 - signal.stop_loss_frac);
         double take_profit_price = next_open * (1 + signal.take_profit_frac);
         account.buyOrder(signal.risk_frac, next_open + next_open_slippage, 
            stop_loss_price, take_profit_price);
      } else if(signal.type == SELL){
         account.full_sell_order(next_open - next_open_slippage);
      }
   }

}

void Backtest::check_candle_against_orders(Account& account, 
   const Candle& candle){
      const std::vector<Position>& positions = account.get_positions();
      std::vector<int> positions_to_close;
      for(int i = 0; i < positions.size(); ++i){
         const Position& current_position = positions.at(i);
         double stop_loss_price = current_position.stop_loss_price;
         double take_profit_price = current_position.take_profit_price;
         
         if(candle.open <= stop_loss_price || candle.open >= take_profit_price){
            double fill_price = calculate_sell_slippage(candle.open);
            account.close_position(current_position.position_id, fill_price);
         } else if(candle.low <= stop_loss_price){
            double fill_price = calculate_sell_slippage(stop_loss_price);
            account.close_position(current_position.position_id, fill_price);
         } else if(candle.high >= take_profit_price){
            double fill_price = calculate_sell_slippage(take_profit_price);
            account.close_position(current_position.position_id, fill_price);
         }
      }
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