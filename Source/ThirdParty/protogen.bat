@echo off
SET PLUGIN_ROOT=%cd%/../..
SET PROTOBUF_PATH=%PLUGIN_ROOT%/Source/ProtobufUE/ThirdParty
SET PROTOC="%PROTOBUF_PATH%/bin/protoc.exe"

cd "%PLUGIN_ROOT%/Source/ChanneldUE"

%PROTOC% --cpp_out=. --cpp_opt=dllexport_decl=CHANNELDUE_API -I "%CHANNELD_PATH%/pkg/channeldpb" -I "%PROTOBUF_PATH%/include" channeld.proto
del /q channeld.pb.cpp
rename channeld.pb.cc channeld.pb.cpp

%PROTOC% --cpp_out=.  --cpp_opt=dllexport_decl=CHANNELDUE_API -I "%CHANNELD_PATH%/pkg/unrealpb" -I "%PROTOBUF_PATH%/include" unreal_common.proto
del /q unreal_common.pb.cpp
rename unreal_common.pb.cc unreal_common.pb.cpp