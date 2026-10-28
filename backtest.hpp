#pragma once

#include <string>
#include "candle.hpp"
#include "account.hpp"
#include <vector>
#include "strategy.hpp"

//loop over all candles, add one by one to strategy
//when strategy executes based on close price, place order on next open
//sell logic is tougher: based on high, low. Take profit at the takeprofit IF
//candle high is greater than takeprofit and same for stoploss. However,
//the market can jump over (ie close above stoploss and open below) in this case, fill at open.



class Backtest {
 private:
  Account persistantAccount;
  std::vector<Candle> candles;
  bool candlesLoaded = false;

  struct BacktestResult{
    std::string strategy_id;
    ParameterSet params;
    std::pair<int,int> period;
    double initial_equity;
    double final_equity;
    //double CAGR;
    //double max_drawdown;
    double sharpe;
    double sortino;
    double volatility;
    int trade_count;
    double win_rate;
    //account snapshot??
  };

 public: 
  Backtest();

  void loadCandles(const std::vector<Candle>& input);
  
  //runs the backtest on own backtest object, returns (preferably) json with data or final balance
  BacktestResult runBacktestPersistant(); //takes params/period
  BacktestResult runBacktestTraining();   //takes params/period creates new account

  //requires: warmup period > slow period > fast period
  BacktestResult runBacktest(Account& account, const ParameterSet& params); //takes params/period/account, makes strat, runs backtest
};


