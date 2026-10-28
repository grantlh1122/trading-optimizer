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
   //next_candle_index should now be period start?
   for(; next_candle_index <= period_end; ++next_candle_index){

   }

}

void Backtest::loadWarmupCandles(const int warmup_period, 
   int& next_candle_index, Strategy& strategy){
   for(int i = 0; i < warmup_period; ++i){
      strategy.loadCandle(candles.at(next_candle_index));
      ++next_candle_index;
   }
}