//what dis do Allows Python to call constructor and functions
#include <pybind11/pybind11.h>
#include <pybind11/numpy.h>

#include <stdexcept>
#include <vector>

#include "backtest.hpp"
#include "candle.hpp"

namespace py = pybind11;

PYBIND11_MODULE(market_module, m){
   py::class_<Backtest>(m, "Backtest")
      .def(py::init<>())
      .def("loadCandles", &Backtest::loadCandles);
      //.def("runTrainingBacktest", &Backtest::runTrainingBacktest)
      //.def("runOptimiedBacktest", &Backtest::runOptimizedBacktest);
}

//TO PUT IN LAMBDA FUNCTION AS A PARAMETER:
void loadCandles(
      py::array_t<double> open_arr,
      py::array_t<double> high_arr,
      py::array_t<double> low_arr,
      py::array_t<double> close_arr,
      py::array_t<int> volume_arr
   ){
      std::cout << "hi" << std::endl;
      candles.clear();

      //what dis do Gets buffer for pointers
      //auto buffer_timestamp = timestamp_arr.request();
      auto buffer_open = open_arr.request();
      auto buffer_high = high_arr.request();
      auto buffer_low = low_arr.request();
      auto buffer_close = close_arr.request();
      auto buffer_volume = volume_arr.request();

      //what dis do Sets pointers to the buffers
      //std::int64_t* ptr_timestamp = static_cast<std::int64_t*>(buffer_timestamp.ptr);
      double* ptr_open = static_cast<double*>(buffer_open.ptr);
      double* ptr_high = static_cast<double*>(buffer_high.ptr);
      double* ptr_low = static_cast<double*>(buffer_low.ptr);
      double* ptr_close = static_cast<double*>(buffer_close.ptr);
      int* ptr_volume = static_cast<int*>(buffer_volume.ptr);

      int num_candles = buffer_close.size;
      candles.reserve(num_candles);

      for(int i = 0; i < num_candles; ++i){
         candles.push_back({
            ptr_open[i], 
            ptr_high[i], 
            ptr_low[i], 
            ptr_close[i], 
            ptr_volume[i]});
      }
   }