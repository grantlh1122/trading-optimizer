#include <cstdint>

struct Candle {
    std::int64_t timestamp;
    double open;
    double high;
    double low;
    double close;
    int volume;
};

//store in PANDAS DATAFRAME 
