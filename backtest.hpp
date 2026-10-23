#pragma once

#include "candle.hpp"
#include "account.cpp"
#include <vector>

//loop over all candles, add one by one to strategy
//when strategy executes based on close price, place order on next open
//sell logic is tougher: based on high, low. Take profit at the takeprofit IF
//candle high is greater than takeprofit and same for stoploss. However,
//the market can jump over (ie close above stoploss and open below) in this case, fill at open.


class Backtest {
 private:
   Account persistantAccount;
   //Strategy* strategy;
   std::vector<Candle> candles;

 public: 
   Backtest(){}

   void loadCandles(const std::vector<Candle>& input);
};


