all: demo_svr demo_cli
demo_svr: ./demo_svr.cpp ../server/dcsrpc.cpp ../share/dcrpc.pb.cc ../share/dcrpc.cpp
	g++ $^ -o $@ --std=c++11 -L ../../lib/ -ldcbase -lprotobuf -O2
demo_cli: ./demo_cli.cpp ../client/dccrpc.cpp ../share/dcrpc.pb.cc ../share/dcrpc.cpp
	g++ $^ -o $@ --std=c++11 -L ../../lib/ -ldcbase -O2 -lprotobuf
clean:
	rm -f demo_cli demo_svr

