#pragma  once
#include "FChanneldThreadUtils.h"
#include "CoreMinimal.h"
#include "GenericPlatform/GenericPlatformProcess.h"

class FChanneldProcWorkerThread;

DECLARE_DELEGATE_TwoParams(FOutputMsgDelegate,FChanneldProcWorkerThread*,const FString&);
DECLARE_MULTICAST_DELEGATE_OneParam(FProcStatusDelegate,FChanneldProcWorkerThread*);


class FChanneldProcWorkerThread : public FChanneldThreadWorker
{
public:
	explicit FChanneldProcWorkerThread(const TCHAR *InThreadName,const FString& InProgramPath,const FString& InParams)
		: FChanneldThreadWorker(InThreadName, []() {}), mProgramPath(InProgramPath), mProgramParams(InParams)
	{}

	virtual uint32 Run()override
	{
		if (FPaths::FileExists(mProgramPath))
		{
			FPlatformProcess::CreatePipe(mReadPipe, mWritePipe);
			// std::cout << TCHAR_TO_ANSI(*mProgramPath) << " " << TCHAR_TO_ANSI(*mPragramParams) << std::endl;

			mProcessHandle = FPlatformProcess::CreateProc(*mProgramPath, *mProgramParams, false, true, true, &mProcessID, 1, NULL, mWritePipe,mReadPipe);
			if (mProcessHandle.IsValid() && FPlatformProcess::IsApplicationRunning(mProcessID))
			{
				if (ProcBeginDelegate.IsBound())
					ProcBeginDelegate.Broadcast(this);
			}

			FString Line;
			while (mProcessHandle.IsValid() && FPlatformProcess::IsApplicationRunning(mProcessID))
			{
				FString NewLine = FPlatformProcess::ReadPipe(mReadPipe);
				if (NewLine.Len() > 0)
				{
					// process the string to break it up in to lines
					Line += NewLine;
					TArray<FString> StringArray;
					int32 count = Line.ParseIntoArray(StringArray, TEXT("\n"), true);
					if (count > 1)
					{
						for (int32 Index = 0; Index < count - 1; ++Index)
						{
							StringArray[Index].TrimEndInline();
							ProcOutputMsgDelegate.ExecuteIfBound(this,StringArray[Index]);
						}
						Line = StringArray[count - 1];
						if (NewLine.EndsWith(TEXT("\n")))
						{
							Line += TEXT("\n");
						}
					}
				}
				FPlatformProcess::Sleep(0.2f);
			}

			int32 ProcReturnCode;
			if (FPlatformProcess::GetProcReturnCode(mProcessHandle,&ProcReturnCode))
			{
				if (ProcReturnCode == 0)
				{
					if(ProcSucceedDelegate.IsBound())
						ProcSucceedDelegate.Broadcast(this);
				}
				else
				{
					if (ProcFailedDelegate.IsBound())
						ProcFailedDelegate.Broadcast(this);
				}
			}
			else
			{
				if (ProcFailedDelegate.IsBound())
					ProcFailedDelegate.Broadcast(this);

			}
			
		}
		mThreadStatus = EChanneldThreadStatus::Completed;
		return 0;
	}
	virtual void Exit()override
	{
		Cancel();
	}
	virtual void Cancel()override
	{
		if (GetThreadStatus() != EChanneldThreadStatus::Busy)
		{
			if (CancelDelegate.IsBound())
				CancelDelegate.Broadcast();
			return;
		}
			
		mThreadStatus = EChanneldThreadStatus::Canceling;
		if (mProcessHandle.IsValid() && FPlatformProcess::IsApplicationRunning(mProcessID))
		{
			FPlatformProcess::TerminateProc(mProcessHandle, true);

			if (ProcFailedDelegate.IsBound())
				ProcFailedDelegate.Broadcast(this);
			mProcessHandle.Reset();
			mProcessID = 0;
		}
		mThreadStatus = EChanneldThreadStatus::Canceled;
		if (CancelDelegate.IsBound())
			CancelDelegate.Broadcast();
	}

	virtual uint32 GetProcessId()const { return mProcessID; }
	virtual FProcHandle GetProcessHandle()const { return mProcessHandle; }

public:
	FProcStatusDelegate ProcBeginDelegate;
	FProcStatusDelegate ProcSucceedDelegate;
	FProcStatusDelegate ProcFailedDelegate;
	FOutputMsgDelegate ProcOutputMsgDelegate;

private:
	FRunnableThread* mThread;
	FString mProgramPath;
	FString mProgramParams;
	void* mReadPipe;
	void* mWritePipe;
	uint32 mProcessID;
	FProcHandle mProcessHandle;
};