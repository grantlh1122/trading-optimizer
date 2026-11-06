#pragma once
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

struct CloseRequest {
    int position_id;
    double fill_price;
};

class Account {
 public:
    //ctors: initial and passing prev orders
    Account();
    Account(double buyingPower);

    void buyOrder(double risk_frac, double price, 
        double stop_loss_price = 0, double take_profit_price = 0);
    //sells all positions of that ticker
    void full_sell_order(double fill_price);
    //sell helper don't use
    void sell_order(std::size_t index, double price);
    //sells given position_id, remove closerequest struct?
    void close_position(CloseRequest close_request);

    int get_trade_count();
    double get_equity(double price);
    int get_win_count();
    int get_num_winning_positions(double price);

    void reset_counters();

    const std::vector<Position>& get_positions() const;

 private:

    double buyingPower = 100'000;
    std::vector<Position> positions;
    int next_position_id = 0;
    //incremented every buy
    int trade_count = 0;
    //incremented every sell
    int win_count = 0;
    //map of tickers to quantity
};



