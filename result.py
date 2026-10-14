"""Result boundary for outputs produced by training and backtesting.

This module is reserved for making run outputs understandable and reviewable.
The output schema, persistence method, charts, and summary metrics remain open
design decisions; this file does not assume any particular format or report.

When those decisions are made, document what each reported value means, which
input run produced it, and any limits that affect interpretation. Keep result
presentation separate from calculations unless the project owner chooses a
different responsibility boundary.

This scaffold has no import-time side effects and does not yet write files or
display results.
"""
