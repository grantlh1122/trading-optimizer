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
  std::vector<Candle> candles;

 public: 
  Backtest(){}

  void loadCandles(const std::vector<Candle>& input);
  
  //runs the backtest on own backtest object, returns (preferably) json with data or final balance
  double runBacktestPersistant(); //takes params, returns account's balance for now
  double runBacktestTraining();   //takes params, returns sharpe ratio
  
  double runBacktest(); //takes params, makes strat, runs backtest, returns sharpe ratio for now
  //todo: design how to get sharpe ratio, how to pass account?? and sharpe ratio at the same time without repeating myself
};


