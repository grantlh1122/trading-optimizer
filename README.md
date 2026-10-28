Quantitative Technical Analysis Parameter Optimizer
Accepts a ticker and runs simulations over 5 years,
running Optuna over each 6 month period to find optimal
parameters, and testing those parameters over the mext 6
momths using a look-ahead bias-free backtester built
for speed an efficiency with C++.

WIP - Docker image