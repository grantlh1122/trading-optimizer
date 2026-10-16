"""Reproducible time and parameter contracts for the optimization layer.

The eventual Optuna objective should create a :class:`TrainingRunPlan` from a
single ``MarketDataSnapshot`` before it begins any trials. The plan carries the
exact candle-time cutoff, the calendar-derived trailing window, and each
parameter change time. It can then be translated directly to the native C++
``BacktestWindow`` and ``ParameterChange`` types by a future binding layer.

No model training, data loading, or Optuna import occurs here. Keeping this
module dependency-free lets the time contract be tested before that adapter is
implemented.
"""

from __future__ import annotations

from dataclasses import dataclass
from datetime import datetime
from math import isfinite
from typing import Iterable

from market import MarketDataSnapshot, UTC, unix_seconds


def subtract_calendar_years(end_utc: datetime, years: int) -> datetime:
    """Return the same UTC calendar time ``years`` earlier.

    February 29 is mapped to February 28 when the target year is not a leap
    year. This explicit calendar rule is preferable to treating every year as
    365 days, which drifts over longer tests.
    """
    if years <= 0:
        raise ValueError("years must be greater than zero")
    normalized_end = end_utc.astimezone(UTC)
    try:
        return normalized_end.replace(year=normalized_end.year - years)
    except ValueError:  # February 29 in a non-leap target year.
        return normalized_end.replace(
            year=normalized_end.year - years, month=2, day=28
        )


@dataclass(frozen=True)
class BacktestTimeWindow:
    """Inclusive candle-close interval that contributes to a backtest result."""

    start_utc: datetime
    end_utc: datetime
    requested_years: int

    def __post_init__(self) -> None:
        if self.start_utc.tzinfo is None or self.end_utc.tzinfo is None:
            raise ValueError("backtest times must be timezone-aware")
        start = self.start_utc.astimezone(UTC)
        end = self.end_utc.astimezone(UTC)
        if start > end:
            raise ValueError("backtest start must not be after its end")
        if self.requested_years <= 0:
            raise ValueError("requested_years must be greater than zero")
        object.__setattr__(self, "start_utc", start)
        object.__setattr__(self, "end_utc", end)

    @classmethod
    def ending_at_snapshot(
        cls, snapshot: MarketDataSnapshot, years: int
    ) -> "BacktestTimeWindow":
        """Build a trailing window ending at the last completed downloaded bar."""
        end = snapshot.last_complete_candle_end_utc
        return cls(subtract_calendar_years(end, years), end, years)

    def native_timestamps(self) -> dict[str, int]:
        """Return the exact UTC Unix-second fields needed by ``BacktestWindow``."""
        return {
            "start_close_timestamp_unix_seconds": unix_seconds(self.start_utc),
            "end_close_timestamp_unix_seconds": unix_seconds(self.end_utc),
        }


@dataclass(frozen=True)
class TrialParameters:
    """One Optuna-selected moving-average and risk configuration.

    Stop and target values are fractions of entry price. Passing ``0.0`` for
    either field is valid and intentionally means that protection is disabled
    for a trade opened with this parameter set.
    """

    short_window_candles: int
    long_window_candles: int
    cash_allocation_fraction: float
    take_profit_fraction: float = 0.0
    stop_loss_fraction: float = 0.0

    def __post_init__(self) -> None:
        if self.short_window_candles <= 0 or self.long_window_candles <= 0:
            raise ValueError("moving-average windows must be greater than zero")
        if self.short_window_candles >= self.long_window_candles:
            raise ValueError("short window must be smaller than long window")
        if not isfinite(self.cash_allocation_fraction) or not (
            0 < self.cash_allocation_fraction <= 1
        ):
            raise ValueError("cash allocation fraction must be in (0, 1]")
        if not isfinite(self.take_profit_fraction) or self.take_profit_fraction < 0:
            raise ValueError("take profit must be a finite non-negative fraction")
        if not isfinite(self.stop_loss_fraction) or not (
            0 <= self.stop_loss_fraction < 1
        ):
            raise ValueError("stop loss must be a finite fraction in [0, 1)")

    def native_fields(self) -> dict[str, float | int]:
        """Names and units expected by the native ``ParameterSet`` adapter."""
        return {
            "short_window_candles": self.short_window_candles,
            "long_window_candles": self.long_window_candles,
            "cash_allocation_fraction": self.cash_allocation_fraction,
            "take_profit_fraction": self.take_profit_fraction,
            "stop_loss_fraction": self.stop_loss_fraction,
        }


@dataclass(frozen=True)
class ParameterEpoch:
    """Parameters activated for signals at or after ``effective_at_utc``."""

    effective_at_utc: datetime
    parameters: TrialParameters

    def __post_init__(self) -> None:
        if self.effective_at_utc.tzinfo is None:
            raise ValueError("parameter effective time must be timezone-aware")
        object.__setattr__(self, "effective_at_utc", self.effective_at_utc.astimezone(UTC))

    def native_fields(self) -> dict[str, float | int]:
        """Return a serializable C++ ``ParameterChange`` representation."""
        return {
            "effective_close_timestamp": unix_seconds(self.effective_at_utc),
            **self.parameters.native_fields(),
        }


@dataclass(frozen=True)
class TrainingRunPlan:
    """All immutable inputs needed to optimize or replay one market snapshot."""

    snapshot: MarketDataSnapshot
    window: BacktestTimeWindow
    parameter_epochs: tuple[ParameterEpoch, ...]

    @classmethod
    def create(
        cls,
        snapshot: MarketDataSnapshot,
        years: int,
        parameter_epochs: Iterable[ParameterEpoch],
    ) -> "TrainingRunPlan":
        return cls(
            snapshot,
            BacktestTimeWindow.ending_at_snapshot(snapshot, years),
            tuple(parameter_epochs),
        )

    def __post_init__(self) -> None:
        if self.window.end_utc > self.snapshot.last_complete_candle_end_utc:
            raise ValueError("backtest window extends beyond downloaded complete data")
        if not self.parameter_epochs:
            raise ValueError("at least one initial parameter epoch is required")
        if self.parameter_epochs[0].effective_at_utc > self.window.start_utc:
            raise ValueError("first parameters must be active at the window start")
        for previous, current in zip(self.parameter_epochs, self.parameter_epochs[1:]):
            if current.effective_at_utc <= previous.effective_at_utc:
                raise ValueError("parameter epochs must be strictly chronological")

    def native_schedule(self) -> list[dict[str, float | int]]:
        """Return parameter entries in the order consumed by ``Backtester.run``."""
        return [epoch.native_fields() for epoch in self.parameter_epochs]
