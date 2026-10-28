#include <cassert>
#include <vector>
#include <map>
#include <variant>
#include "candle.hpp"
#include <string>
#include <deque>


using ParameterValue = std::variant<int, double, bool, std::string>;
using ParameterSet = std::map<std::string, ParameterValue>;

enum OrderType{
    BUY = 1,
    HOLD = 0,
    SELL = -1
};

struct MovingAverageCrossoverParams{
    const int fast_period;
    const int slow_period;
    const double risk_frac;
    const double stop_loss_frac;
    const double take_profit_frac;
};

struct OrderRequest{
    const OrderType type;
    const double risk_frac = -1;
    const double stop_loss_percent = -1;
    const double take_profit_percent = 1;
};


class Strategy{
 public:
    //will take strategy id and factory it with parameter set
    virtual OrderRequest generateSignal(const Candle& nextCandle) = 0;
    virtual void loadCandle(const Candle& nextCandle);

 private:
    std::vector<Candle> candles;
};

class MovingAverageCrossoverStrategy : public Strategy {
 public:
    MovingAverageCrossoverStrategy(int fast, int slow, double risk, 
        double stop_loss, double take_profit);

    OrderRequest generateSignal(const Candle& nextCandle) override;
    void loadCandle(const Candle& nextCandle) override;

 private: 
    const int fast_periods;
    const int slow_periods;
    const double risk_frac;
    const double stop_loss_frac;
    const double take_profit_frac;


    double fast_sum = 0;
    double slow_sum = 0;
    double fast_MA = 0;
    double slow_MA = 0;
    std::deque<double> fast_closes;
    std::deque<double> slow_closes;
    
};

//Todo: error check params
Strategy* strategyFactory(const std::string& strategy_id, ParameterSet params);