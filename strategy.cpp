#include <cassert>
#include <vector>
#include "candle.hpp"
//from a design perspetive specific strat params should be here.
   
//Flow:
//Backtester creates account and strategy object, passes a bunch of candles to strategy (1 year)
//Backtester starts. for each candle:
//Backtester asks for signal from strategy. Strategy generates order request. 
//Do it at the price of the next open.
//Then backtester passes the order -1 or 1 to the account. The account will attempt to buy/sell this number. 
//account will always sell the given quantity or as many as possible, same goes for buying.
//Account will turn this into a position, which will be checked every candle by the backtest.
//If it was a take profit or stoploss hit, determine if it
//happened (1) before open (sell at open) or (2) during candle (sell high/low)

//To be designed:
//each time the backtest is run, it will be on a set period. need system to keep that consistent, and to research how candles work.
//either keep one backtest object for all of the live test and hotswap params (recommend), or somehow make
//a new instance every 6 months and copy account over. What's easier, faster, how would i get account?
//maybe one function runs the backtest and dies after the period, returning sharpe ratio, one returns account.
//or maybe it just returns a backtestresult object or json, the training gets the sharpe ratio and the 
//main program that gets the parameters could call the backtest on different periods and recreate each account.
//that sounds like a pain in the ass though, maybe just keep the backtest alive whole time?

// abstract class to be inherited from? ho
class Strategy{
 public:
    virtual int generateSignal();
 private:
    std::vector<Candle> candles;
    double param1;
    double param2;
    double stoploss_param;
    double takeprofit_param;
    //etc
};

Strategy* strategyFactory(){
    assert(false);
}

//pitfall: how do we change parameters? shouldn't change candles, in fact should keep all candles and stuff like MA
//when we change parameters, there will be variables to change in a function. 
//im thinking, every time you backtest you pass in 1 year prior of candles instantly, just to determine indicators.
//then run 6 months of candles, actually simulating trades. If training, return sharpe ratio to optuna. 
//if live trading, pass the candles and hotswap parameters (on close?)
//the main backtesting object should pass in the same candles as the training ones (not at the same time.
//training happens thousands of times on the same period). Choose one or the other to do first.
//prob easier to get newest params (if not first period), simulate the one backtest (using one backtest object that has 
//every candle). then, run the training on that same period (many times with many different parameters) on a new backtest
//object each time. this means we somehow have to determine which candles the backtester gets (adding the precandles)
//and where we are in time. TRY to get codex to turn this into a program flow file.

//1 warmup year, 4 years of training/ trading. 6 month periods means 8 training periods and 7 trading, assuming we continue 
//training up till end to display final reccomended params.