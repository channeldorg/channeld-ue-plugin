#pragma once
#include "Common/TcpSocketBuilder.h"
#include "Interfaces/IPv4/IPv4Endpoint.h"

class CHANNELDEDITOR_API ChanneldEditorUtils
{
public:
	static bool IsPortInUse(int32 Port)
	{
		bool bIsInUse = false;
		TSharedRef<FInternetAddr> RemoteAddr = ISocketSubsystem::Get()->CreateInternetAddr();
		bool IsIpValid;
		RemoteAddr->SetIp(TEXT("127.0.0.1"), IsIpValid);
		RemoteAddr->SetPort(Port);
		FSocket* Socket = ISocketSubsystem::Get()->CreateSocket(NAME_Stream, TEXT("CheckPort"), RemoteAddr->GetProtocolType());
		if (Socket->Connect(*RemoteAddr))
		{
			bIsInUse = true;
			Socket->Close();
			ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(Socket);
		}
		return bIsInUse;
	}
};
