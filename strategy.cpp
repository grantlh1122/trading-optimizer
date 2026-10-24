#include <cassert>
#include <vector>
#include "candle.hpp"

// abstract class to be inherited from? how 

struct MovingAverageCrossoverParams{
    int fast_period;
    int slow_period;
    double risk_frac;
    double stoploss_param;
    double takeprofit_param;
};


class Strategy{
 public:
    //will take strategy id and factory it with parameter set
    virtual int generateSignal();
 private:
    std::vector<Candle> candles;

};

Strategy* strategyFactory(){
    assert(false);
}



