demo_svr: ./demo_server.cpp ../server/dcsrpc.cpp
	g++ $^ -o $@ --std=c++11 -L ../../lib/ -ldcbase
demo_cli: ./demo_cli.cpp ../client/dccrpc.cpp
	g++ $^ -o $@ --std=c++11 -L ../../lib/ -ldcbase -g


