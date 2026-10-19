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

enum OrderType{
    BUY = 1,
    SELL = -1
};


struct Position{
    //for if I want to add short/long
    //enum class Side { Long = 1, Short = -1};
    //Side side; 
    int quantity = -1;
    double entry_price = -1;
    // if 0, no stop loss / take profit for that order
    double stop_loss_price = -1;
    double take_profit_price = -1;
    
    Position(int qty, double price, double stop_loss_price, double take_profit_price) : 
    quantity(qty), entry_price(price), stop_loss_price(stop_loss_price), take_profit_price(take_profit_price){}

};


class Account {
 public:
    //ctors: initial and passing prev orders
    Account();
    Account();

    //TODO: pass in a value that allows dynamic sizing based on volatility of different tickers
    //this will require a system that allows multiple tickers
    //TODO: assert bad inputs
    void placeOrder(double risk_frac, double price, OrderType type, double stop_loss_price = 0, double take_profit_price = 0){
        if (type == BUY){
            buyOrder(risk_frac, price, stop_loss_price, take_profit_price);
        }
        if (type == SELL){
            sellOrder(price);
        }
    }

    //calculates how many you're willing to buy with risk fraction.
    //then buys that many or as many as can be afforded.
    void buyOrder(double risk_frac, double price, double stop_loss_price, double take_profit_price){
        int qty = (int)((buyingPower * risk_frac) / (price - stop_loss_price));
        int qty_afforded = (int)(buyingPower / price);
        qty = std::min(qty, qty_afforded);

        if(qty > 0){
            positions.push_back(Position(qty, price, stop_loss_price, take_profit_price));
        }
    }

    //sells all positions TODO: of a given ticker
    void sellOrder(double price){
        for(int i = positions.size(); i >= 0; --i){
            buyingPower += price * positions[i].quantity;
            positions.erase(positions.begin() + i);
        }
    }

 private:
    double buyingPower = 100'000;
    std::vector<Position> positions;
    int quantity = 0;
};




