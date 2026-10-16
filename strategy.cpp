#include <cassert>
#include <vector>
#include "candle.hpp"

// abstract class to be inherited from? how 
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

