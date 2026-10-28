#pragma once
//#include <cstdint>

struct Candle {
    //todo: add date to each candle
    //std::int64_t timestamp;
    const double open;
    const double high;
    const double low;
    const double close;
    const int volume;
};

//store in PANDAS DATAFRAME 
