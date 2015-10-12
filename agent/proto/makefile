PROTO_GEN=dagent.pb.h
PROTO_SRC=dagent.proto

${PROTO_GEN}:${PROTO_SRC}
	protoc --cpp_out=./ -I ./ $<
