#include <vector>
#include <iostream>
#include <cassert>



class Account {
 public:
    //ctors: initial and passing prev orders
    Account();
    Account(double buyingPower);

    void buyOrder(double risk_frac, double price, double stop_loss_price = 0, double take_profit_price = 0);
    void sellOrder(double price);

 private:
    struct Position{
        //for if I want to add short/long
        //enum class Side { Long = 1, Short = -1};
        //Side side; 
        int quantity = -1;
        double entry_price = -1;
        // if 0, no stop loss / take profit for that order
        double stop_loss_price = -1;
        double take_profit_price = -1;
        //if any value here is -1, something went wrong.

        Position(int qty, double price, double stop_loss_price, double take_profit_price) : 
        quantity(qty), entry_price(price), stop_loss_price(stop_loss_price), take_profit_price(take_profit_price){}

    };

    static constexpr double SLIPPAGE_RATE = 0.001;
    double buyingPower = 100'000;
    std::vector<Position> positions;
    //map of tickers to quantity
};



