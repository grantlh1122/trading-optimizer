#include <vector>
#include <iostream>
#include <cassert>

struct Position{
    //for if I want to add short/long
    //enum class Side { Long = 1, Short = -1};
    //Side side; 
    int position_id;
    int quantity;
    double entry_price;
    double stop_loss_price;
    double take_profit_price;
};

class Account {
 public:
    //ctors: initial and passing prev orders
    Account();
    Account(double buyingPower);

    void buyOrder(double risk_frac, double price, 
        double stop_loss_price = 0, double take_profit_price = 0);
    void full_sell_order(double fill_price);
    void sell_order(int index, double price);
    void close_position(int position_id, double fill_price);

    const std::vector<Position>& get_positions() const;
    
 private:

    double buyingPower = 100'000;
    std::vector<Position> positions;
    int next_position_id = 0;
    //map of tickers to quantity
};



