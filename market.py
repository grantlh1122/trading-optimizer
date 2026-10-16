"""Timestamped market-data contracts used by training and backtesting.

This module deliberately does not import the Alpaca SDK or make a network
request. The application entry point should download all requested bars once,
convert them to :class:`Candle`, and create one :class:`MarketDataSnapshot`.
Every later operation receives that immutable snapshot. This prevents an
optimization run from accidentally training on data that arrived after an
earlier trial began.

Times are timezone-aware UTC datetimes. The native C++ layer uses UTC Unix
seconds, so callers must preserve a bar's real start and end rather than use a
local display time or the wall-clock time at which it was downloaded.
"""

from __future__ import annotations

from dataclasses import dataclass
from datetime import datetime, timezone
from math import isfinite
from typing import Iterable


UTC = timezone.utc


def _as_utc(value: datetime, field_name: str) -> datetime:
    """Return *value* in UTC, rejecting naive datetimes before they leak in."""
    if value.tzinfo is None or value.utcoffset() is None:
        raise ValueError(f"{field_name} must be timezone-aware")
    return value.astimezone(UTC)


def unix_seconds(value: datetime) -> int:
    """Convert an exact-second UTC datetime to the C++ timestamp contract.

    Hourly and daily Alpaca bars normally meet this requirement. Rejecting
    subsecond values is safer than silently truncating a timestamp and changing
    an order of events.
    """
    normalized = _as_utc(value, "timestamp")
    if normalized.microsecond != 0:
        raise ValueError("native timestamps require an exact whole second")
    return int(normalized.timestamp())


@dataclass(frozen=True)
class Candle:
    """One completed OHLC candle, including whether the exchange was open.

    ``end_utc`` is the time at which the close became known. For standard
    Alpaca bars, use the bar interval to derive it from the provider timestamp
    when the provider reports only a start time. Do not use download time.
    """

    start_utc: datetime
    end_utc: datetime
    open_price: float
    high_price: float
    low_price: float
    close_price: float
    market_is_open: bool = True

    def __post_init__(self) -> None:
        start = _as_utc(self.start_utc, "candle start")
        end = _as_utc(self.end_utc, "candle end")
        if start >= end:
            raise ValueError("candle start must be before candle end")
        prices = (self.open_price, self.high_price, self.low_price, self.close_price)
        if any(not isfinite(price) or price <= 0 for price in prices):
            raise ValueError("OHLC prices must be finite and greater than zero")
        if self.high_price < max(self.open_price, self.close_price):
            raise ValueError("candle high is below its open or close")
        if self.low_price > min(self.open_price, self.close_price):
            raise ValueError("candle low is above its open or close")
        object.__setattr__(self, "start_utc", start)
        object.__setattr__(self, "end_utc", end)

    def native_fields(self) -> dict[str, float | int | bool]:
        """Return the field names expected by a thin C++ binding adapter."""
        return {
            "open_timestamp_unix_seconds": unix_seconds(self.start_utc),
            "close_timestamp_unix_seconds": unix_seconds(self.end_utc),
            "open_price_dollars": self.open_price,
            "high_price_dollars": self.high_price,
            "low_price_dollars": self.low_price,
            "close_price_dollars": self.close_price,
            "market_is_open": self.market_is_open,
        }


@dataclass(frozen=True)
class MarketDataSnapshot:
    """The complete, immutable market-data input for one symbol and one run.

    ``loaded_at_utc`` is audit metadata only. ``last_complete_candle_end_utc``
    is the authoritative data cutoff used to calculate a trailing N-year
    backtest window. It is intentionally not derived from ``datetime.now()``.
    """

    symbol: str
    loaded_at_utc: datetime
    candles: tuple[Candle, ...]

    def __post_init__(self) -> None:
        if not self.symbol.strip():
            raise ValueError("symbol must not be empty")
        object.__setattr__(self, "symbol", self.symbol.upper())
        object.__setattr__(
            self, "loaded_at_utc", _as_utc(self.loaded_at_utc, "loaded_at")
        )
        object.__setattr__(self, "candles", tuple(self.candles))
        if not self.candles:
            raise ValueError("a market-data snapshot must contain at least one candle")
        for previous, current in zip(self.candles, self.candles[1:]):
            if current.end_utc <= previous.end_utc:
                raise ValueError("candle end times must be strictly increasing")

    @classmethod
    def from_candles(
        cls, symbol: str, loaded_at_utc: datetime, candles: Iterable[Candle]
    ) -> "MarketDataSnapshot":
        """Convenience constructor that consumes an Alpaca-adapter iterable once."""
        return cls(symbol=symbol, loaded_at_utc=loaded_at_utc, candles=tuple(candles))

    @property
    def first_candle_start_utc(self) -> datetime:
        return self.candles[0].start_utc

    @property
    def last_complete_candle_end_utc(self) -> datetime:
        return self.candles[-1].end_utc

    def native_candles(self) -> list[dict[str, float | int | bool]]:
        """Expose ordered, lossless native-adapter records without fetching data."""
        return [candle.native_fields() for candle in self.candles]
