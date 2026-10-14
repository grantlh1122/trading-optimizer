# Trading Optimizer

This repository is a draft for a program that will use machine learning to
optimize technical-analysis parameters across changing time periods and
different funds.

The project design is still being researched. This document describes the
current file layout, not a finalized data pipeline or trading strategy. The
project owner will decide the input sources, data formats, model, evaluation
rules, and execution details before those are implemented.

## File layout

- `market.py` — planned home for market-data input and related preparation.
  The exact source, schema, validation rules, and cleaning steps remain to be
  decided.
- `backtest.cpp` — planned home for backtesting calculations and simulation.
  Its interface with the Python files, supported trading rules, and accounting
  behavior remain to be designed.
- `training.py` — planned home for model training and parameter selection.
  The model, features, objective, and training procedure are not selected yet.
- `result.py` — planned home for collecting and presenting outputs from a
  training or backtest run. The result format and reporting needs remain open.
- `Makefile` — reserved for documented build or development commands once the
  project has concrete build steps.
- `.gitignore` — excludes generated files that should not be committed.

## Draft flow

The intended high-level flow is market data into a backtest or training step,
followed by results that can be reviewed. This is only a sketch of the
components: it does not yet define how data moves between them or which
component owns shared calculations.

## Development status

The files are currently a documented scaffold. They intentionally contain no
data-source integration, model implementation, trading rules, or performance
claims. As decisions are made, document the chosen behavior in the relevant
module before adding implementation so each part can be reviewed on its own.
