

proto/mongo.pb.h testapi/test.pb.h: proto/mongo.proto testapi/test.proto
	cd proto && protoc mongo.proto --cpp_out=.
	cd testapi && protoc test.proto --cpp_out=.


clean:
	rm -f proto/*.pb.*
	rm -f testapi/*.pb.*
