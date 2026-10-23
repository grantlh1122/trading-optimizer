#todo: get candles, configure timeframes, run backtest live and optuna which will run with the same backtest object to save candle memory
#todo: wrap this driver in a file that gets inputs and makes the outputs look pretty. 
#todo: find a way to get data from each individual timeframe from optuna.

import numpy as np
import yfinance as yf
import market_module

def main():
    engine = market_module.Backtest()

    df = yf.download("AAPL", period="1mo", interval="1h")
    df.columns = df.columns.get_level_values(0)
    df = df.dropna()

    engine.loadCandles(
        df['Open'].to_numpy(dtype=np.float64),
        df['High'].to_numpy(dtype=np.float64),
        df['Low'].to_numpy(dtype=np.float64),
        df['Close'].to_numpy(dtype=np.float64),
        df['Volume'].to_numpy(dtype=np.int32)
    )

if __name__ == "__main__":
    main()