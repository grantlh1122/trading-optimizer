#include "backtest.hpp"
//loop over all candles, add one by one to strategy
//when strategy executes based on close price, place order on next open
//sell logic is tougher: based on high, low. Take profit at the takeprofit IF
//candle high is greater than takeprofit and same for stoploss. However,
//the market can jump over (ie close above stoploss and open below) in this case, fill at open.



   Backtest::Backtest() = default;

   void Backtest::loadCandles(const std::vector<Candle>& input) {
      candles = input;
   }


//what dis do Allows Python to call constructor and functions

