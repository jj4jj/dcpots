all: demo_svr demo_cli
demo_svr: ./demo_server.cpp ../server/dcsrpc.cpp ../share/dcrpc.pb.cc ../share/dcrpc.cpp
	g++ $^ -o $@ --std=c++11 -L ../../lib/ -ldcbase -lprotobuf
demo_cli: ./demo_cli.cpp ../client/dccrpc.cpp ../share/dcrpc.pb.cc ../share/dcrpc.cpp
	g++ $^ -o $@ --std=c++11 -L ../../lib/ -ldcbase -g -lprotobuf


