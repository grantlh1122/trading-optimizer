#include <strategy.hpp>
#include <cassert>


Strategy* strategyFactory(const std::string& strategy_id, ParameterSet params){
    if(strategy_id != "mac"){assert(false);};

    double risk_frac = std::get<double>(params.at("risk_frac"));
    double stop_loss_frac = std::get<double>(params.at("stop_loss_frac"));
    double take_profit_frac = std::get<double>(params.at("take_profit_frac"));

    if(strategy_id == "mac"){
        int fastPeriod = std::get<int>(params.at("fast_period"));
        int slowPeriod = std::get<int>(params.at("slow_period"));

        return new MovingAverageCrossoverStrategy(fastPeriod, slowPeriod, 
            risk_frac, stop_loss_frac, take_profit_frac);
    }
}

//MAC Strategy

OrderRequest MovingAverageCrossoverStrategy::generateSignal(const Candle& nextCandle){
    double old_fast_MA = fast_MA;
    double old_slow_MA = slow_MA;
    loadCandle(nextCandle);

    if(old_fast_MA <= old_slow_MA && fast_MA > slow_MA){
        return {BUY, risk_frac, stop_loss_frac, take_profit_frac};
    } else if(old_fast_MA >= old_slow_MA && fast_MA < slow_MA){
        return {SELL};
    } else {
        return {HOLD};
    } 
}

void MovingAverageCrossoverStrategy::loadCandle(const Candle& nextCandle){
    const double close_price = nextCandle.close;
    fast_closes.push_back(close_price);
    slow_closes.push_back(close_price);

    if(fast_closes.size() > fast_periods){
        fast_sum -= fast_closes.front();
        fast_closes.pop_front();
    }
    if(slow_closes.size() > slow_periods){
        slow_sum -= slow_closes.front();
        slow_closes.pop_front();
    }

    fast_sum += close_price;
    slow_sum += close_price;

    fast_MA = fast_sum / fast_closes.size();
    slow_MA = slow_sum / slow_closes.size();
}