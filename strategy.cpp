#include <strategy.hpp>
#include <cassert>


Strategy* strategyFactory(const std::string& strategy_id, ParameterSet params){
    if(strategy_id != "mac"){assert(false);};

    double risk_frac = std::get<double>(params.at("risk_frac"));
    double stop_loss_frac = std::get<double>(params.at("stop_loss_frac"));
    double take_profit_frac = std::get<double>(params.at("stop_loss_frac"));

    if(strategy_id == "mac"){
        int fastPeriod = std::get<int>(params.at("fast_period"));
        int slowPeriod = std::get<int>(params.at("slow_period"));

        return new MovingAverageCrossoverStrategy(fastPeriod, slowPeriod, 
            risk_frac, stop_loss_frac, take_profit_frac);
    }
}

//MAC Strategy

