PROTO_GEN=dcnode.pb.h
PROTO_SRC=dcnode.proto

${PROTO_GEN}:${PROTO_SRC}
	protoc --cpp_out=./ -I ./ $<
