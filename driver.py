#todo: get candles, configure timeframes, run backtest live and optuna which will run with the same backtest object to save candle memory
#todo: wrap this driver in a file that gets inputs and makes the outputs look pretty. 
#todo: find a way to get data from each individual timeframe from optuna.

import yfinance as yf
import market_module

def main():
    engine = market_module.Backtest()

    df = yf.download("AAPL", period = "1mo", interval = "1hr")
    df.columns = df.columns.get_level_values(0)
    df = df.dropna()

    engine.loadCandles(
        df['Open'].to_numpy(),
        df['High'].to_numpy(),
        df['Low'].to_numpy(),
        df['Close'].to_numpy(),
        df['Volume'].to_numpy
    )