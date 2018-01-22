PROTO_GEN=dcagent.pb.h
PROTO_SRC=dcagent.proto

${PROTO_GEN}:${PROTO_SRC}
	protoc --cpp_out=./ -I ./ $<
