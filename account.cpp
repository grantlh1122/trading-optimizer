#include <vector>
#include <iostream>
#include <cassert>

//money and orders go here
/*

backtester will make an account object and a 
strategy object ptr and store it as a member var

this will store the current cash balance and
qty of the stock (1 for now) passing in price
can calculate total balance.

order logic is placed here, no stop loss yet

so whats in backtester? the loop:
initialize the account and strategy. load
all the candles. pass in a candle to the strategy, 
get signal, if buy, place order thru user,
if sell, place order thru user, else hold
based on OHLC.
There will be many backtester objects. 5 years:
training for 6 month sprints for whole time,
so 10 different periods of backtests with 
thousands of individual backtests. somehow
avoid copying the data, pass by ref. once a back
test is done while training, delete it, therell 
only need to be one at a time.
then the simulated "live" bot. runs tests on every
6mth period except the first. this one needs
to have the same user/strat passed on, parameters
will change but thats it. maybe overload ctor 
can do this. anyways the live bot, in 5 years will
run only 9 individual times, all with the same 
account.
*/

class Account {
 public:
    //ctors: initial
    Account();

    int placeOrder(){
        assert(false);
    }

 private:
    double buyingPower = 100'000;
    std::vector<Position> positions;
};

struct Position{
    //for if I want to add short/long
    //enum class Side { Long = 1, Short = -1};
    //Side side; 
    int quantity;
    double entry_price;
    double stop_loss_price;
    double take_profit_price;
    
    Position(int qty, double price){
        this->quantity = qty;
        this->entry_price = price;
    }
    Position(int qty, double price, double stop_loss_price, double take_profit_price){
        this->quantity = qty;
        this->entry_price = price;
        this->stop_loss_price = stop_loss_price;
        this->take_profit_price = take_profit_price;
    }
};

