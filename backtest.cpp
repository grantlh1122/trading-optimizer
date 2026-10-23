#include "candle.hpp"
#include "account.cpp"
#include <vector>
#include <pybind11/pybind11.h>
#include <pybind11/numpy.h>
//loop over all candles, add one by one to strategy
//when strategy executes based on close price, place order on next open
//sell logic is tougher: based on high, low. Take profit at the takeprofit IF
//candle high is greater than takeprofit and same for stoploss. However,
//the market can jump over (ie close above stoploss and open below) in this case, fill at open.

namespace py = pybind11;

class Backtest {
 private:
   Account persistantAccount;
   //Strategy* strategy;
   std::vector<Candle> candles;

 public: 
   Backtest() = default;

   std::vector<Candle> loadCandles(
      py::array_t<std::int64_t> timestamp_arr,
      py::array_t<double> open_arr,
      py::array_t<double> high_arr,
      py::array_t<double> low_arr,
      py::array_t<double> close_arr,
      py::array_t<int> volume_arr
   ){
      candles.clear();

      //what dis do Gets buffer for pointers
      auto buffer_timestamp = timestamp_arr.request();
      auto buffer_open = open_arr.request();
      auto buffer_high = high_arr.request();
      auto buffer_low = low_arr.request();
      auto buffer_close = close_arr.request();
      auto buffer_volume = volume_arr.request();

      //what dis do Sets pointers to the buffers
      std::int64_t* ptr_timestamp = static_cast<std::int64_t*>(buffer_timestamp.ptr);
      double* ptr_open = static_cast<double*>(buffer_open.ptr);
      double* ptr_high = static_cast<double*>(buffer_high.ptr);
      double* ptr_low = static_cast<double*>(buffer_low.ptr);
      double* ptr_close = static_cast<double*>(buffer_close.ptr);
      int* ptr_volume = static_cast<int*>(buffer_volume.ptr);

      int num_candles = buffer_close.size;
      candles.reserve(num_candles);

      for(int i = 0; i < num_candles; ++i){
         candles.push_back(
            {ptr_timestamp[i], 
            ptr_open[i], 
            ptr_high[i], 
            ptr_low[i], 
            ptr_close[i], 
            ptr_volume[i]});
      }
   }
};

//what dis do Allows Python to call constructor and functions
PYBIND11_MODULE(market_module, m){
   py::class_<Backtest>(m, "Backtest")
      .def(py::init<>())
      .def("loadCandles", &Backtest::loadCandles);
      //.def("runTrainingBacktest", &Backtest::runTrainingBacktest)
      //.def("runOptimiedBacktest", &Backtest::runOptimizedBacktest);
}
