#include "account.hpp"

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

//ctors: initial and passing prev orders
Account::Account() = default;
Account::Account(double buyingPower) : buyingPower(buyingPower) {};

//TODO: pass in a value that allows dynamic sizing based on volatility of different tickers
//this will require a system that allows multiple tickers


//calculates how many you're willing to buy with risk fraction.
//then buys that many or as many as can be afforded.
//TODO: position invariants
void Account::buyOrder(double risk_frac, double price, double stop_loss_price, double take_profit_price){
    assert(risk_frac >= 0 && risk_frac <= 1 && price >= 0 && stop_loss_price >= 0 && take_profit_price >= 0);
    assert(stop_loss_price <= price && take_profit_price >= price);

    int qty_want = (int)((buyingPower * risk_frac) / (price - stop_loss_price));
    int qty_afforded = (int)(buyingPower / price);
    int qty_to_buy = std::min(qty_want, qty_afforded);

    if(qty_to_buy > 0){
        positions.push_back({next_position_id, qty_to_buy, 
            price, stop_loss_price, take_profit_price});
        buyingPower -= qty_to_buy * price;
        ++next_position_id;
    }
}

//sells all positions TODO: of a given ticker, Slippage
void Account::full_sell_order(double fill_price){
    for(std::size_t i = positions.size(); i > 0; --i){
        sell_order(i - 1, fill_price);
    }
}

void Account::sell_order(std::size_t index, double price){
    buyingPower += price * positions[index].quantity;
    positions.erase(positions.begin() + index);
}

double Account::get_equity(double price){
    double equity = buyingPower;
    for(int i = 0; i < positions.size(); ++i){
        equity += positions[i].quantity * price;
    }
    return equity;
}


//could make this faster with a hashmap from O(n) to O(1)
void Account::close_position(CloseRequest close_request){
    for(int i = 0; i < positions.size(); ++i){
        if(positions.at(i).position_id == close_request.position_id){
            sell_order(i, close_request.fill_price);
            return;
        }
    }
}

const std::vector<Position>& Account::get_positions() const{
    return positions;
}
/*
int main(){

    Account Grant;
    Grant.buyOrder(0.01, 100, 90, 110);
    Grant.sellOrder(150);
    Grant.buyOrder(0.01, 100, 90, 110);
    Grant.buyOrder(0.01, 100, 90, 110);
    
}

*/
