#pragma once
//#include <cstdint>

struct Candle {
    //todo: add date to each candle
    //std::int64_t timestamp;
    //pass candle by const ref
    double open;
    double high;
    double low;
    double close;
    int volume;
};

//store in PANDAS DATAFRAME 
