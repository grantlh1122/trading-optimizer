#include "backtest.hpp"
#include <vector>
#include <fstream>
#include <sstream>

//perhaps this could be a backtest function, but for now we can load
//candles here. For further tests I will need to construct candles to test edge
//cases and slippage.

std::vector<Candle> read_candles_from_csv(const std::string& filename){
    std::ifstream fin(filename);
    if(!fin){
        std::cout << "couldn't open file" << std::endl;
        assert(false); //for now
    }

    std::string line; 
    std::getline(fin, line);
    std::vector<Candle> candles;

    while(std::getline(fin, line)){
        std::stringstream row(line);
        std::string close, high, low, open, volume;

        std::getline(row, close, ',');
        std::getline(row, high, ',');
        std::getline(row, low, ',');
        std::getline(row, open, ',');
        std::getline(row, volume, ',');

        candles.push_back({std::stod(open), std::stod(high),
            std::stod(low), std::stod(close), stoi(volume)});
    }

    return candles;
}

int main(){
    std::vector<Candle> candles = 
        read_candles_from_csv("SPY_1h_6mo.csv");
    Backtest backtest;
    backtest.loadCandles(candles);
    ParameterSet params = {
        {"fast_period", 50}, 
        {"slow_period", 200}, 
        {"risk_frac", 0.02},
        {"stop_loss_frac", 0.02},
        {"take_profit_frac", 0.04} 
    };
    BacktestResult result = backtest.runBacktestTraining
        ("mac", params, 400, 890, 400);

    double bars_per_year = 252 * 7;
    double annual_sharpe = result.sharpe * std::sqrt(bars_per_year);
    std::cout << "annual sharpe: " << annual_sharpe << std::endl;
}