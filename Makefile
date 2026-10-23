.PHONY: backtest

backtest:
	c++ -O3 -Wall -shared -std=c++17 -fPIC $$(.venv/bin/python -m pybind11 --includes) backtest.cpp -o "market_module$$(.venv/bin/python -c 'import sysconfig; print(sysconfig.get_config_var("EXT_SUFFIX"))')"
