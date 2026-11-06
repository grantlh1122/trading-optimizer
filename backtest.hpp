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
  static constexpr double SLIPPAGE_RATE = 0.001;

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

    BacktestResult(const std::string& id, 
      const ParameterSet& param, std::pair<int, int> period, double start_equity) : 
        params(param), period(period), initial_equity(start_equity) {}
  };

 public: 
  Backtest();

  void loadCandles(const std::vector<Candle>& input);
  void loadWarmupCandles(const int warmup_period, 
    int& next_candle_index, Strategy& strategy);
  
  //runs the backtest on own backtest object, returns (preferably) json with data or final balance
  BacktestResult runBacktestPersistant(const std::string& strategy_id, const ParameterSet& params,
    const int period_start, const int period_end, const int warmup_period); //takes params/period
  BacktestResult runBacktestTraining(const std::string& strategy_id, const ParameterSet& params,
    const int period_start, const int period_end, const int warmup_period);   //takes params/period creates new account

  //requires: warmup period > slow period > fast period
  BacktestResult runBacktest(Account& account, const std::string& strategy_id, const ParameterSet& params,
    const int period_start, const int period_end, const int warmup_period); //takes params/period/account, makes strat, runs backtest

    void check_candle_against_orders(Account& account, 
      const Candle& candle);

    double calculate_sell_slippage(double price);

};

