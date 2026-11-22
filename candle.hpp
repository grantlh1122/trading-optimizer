#pragma once
#include <cstdint>

struct Candle {
    //pass candle by const ref
    std::int64_t timestamp;
    double open;
    double high;
    double low;
    double close;
    int volume;
};

//store in PANDAS DATAFRAME 
