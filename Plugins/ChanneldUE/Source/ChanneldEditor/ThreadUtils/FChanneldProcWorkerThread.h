#pragma  once
#include "FChanneldThreadUtils.h"
#include "CoreMinimal.h"
#include "GenericPlatform/GenericPlatformProcess.h"

class FChanneldProcWorkerThread;

DECLARE_DELEGATE_TwoParams(FOutputMsgDelegate, FChanneldProcWorkerThread*, const FString&);
DECLARE_MULTICAST_DELEGATE_OneParam(FProcStatusDelegate, FChanneldProcWorkerThread*);


class FChanneldProcWorkerThread : public FChanneldThreadWorker
{
public:
	explicit FChanneldProcWorkerThread(const TCHAR* InThreadName, const FString& InProgramPath, const FString& InParams)
		: FChanneldThreadWorker(InThreadName, []()
		{
		}), mProgramPath(InProgramPath), mProgramParams(InParams)
	{
	}

	explicit FChanneldProcWorkerThread(const TCHAR* InThreadName, const FString& InProgramPath, const FString& InParams, const FString& InWorkingDirectory)
		: FChanneldThreadWorker(InThreadName, []()
		{
		}), mProgramPath(InProgramPath), mProgramParams(InParams), WorkingDirectory(InWorkingDirectory)
	{
	}

	virtual ~FChanneldProcWorkerThread() override
	{
		Exit();
	}


	virtual uint32 Run() override
	{
		if (true || FPaths::FileExists(mProgramPath))
		{
			FPlatformProcess::CreatePipe(mReadPipe, mWritePipe);
			// std::cout << TCHAR_TO_ANSI(*mProgramPath) << " " << TCHAR_TO_ANSI(*mPragramParams) << std::endl;

			mProcessHandle = FPlatformProcess::CreateProc(
				*mProgramPath, *mProgramParams,
				false, true, true,
				&mProcessID, 1,
				WorkingDirectory.IsEmpty() ? nullptr : *WorkingDirectory,
				mWritePipe, mReadPipe
			);
			if (mProcessHandle.IsValid() && FPlatformProcess::IsApplicationRunning(mProcessID))
			{
				if (ProcBeginDelegate.IsBound())
					ProcBeginDelegate.Broadcast(this);
			}

			FString Output;
			auto ReadOutput = [this, &Output]()
			{
				FString NewOutput = FPlatformProcess::ReadPipe(mReadPipe);
				if (NewOutput.Len() > 0)
				{
					// process the string to break it up in to lines
					Output += NewOutput;
					TArray<FString> SubLines;
					const int32 Count = Output.ParseIntoArray(SubLines, TEXT("\n"), true);
					if (Count > 1)
					{
						for (int32 Index = 0; Index < Count; ++Index)
						{
							if (Index == Count - 1)
							{
								if(!NewOutput.EndsWith(TEXT("\n")))
								{
									Output = SubLines[Index];
									continue;
								}
								Output = TEXT("");
							}
							SubLines[Index].TrimEndInline();
							ProcOutputMsgDelegate.ExecuteIfBound(this, SubLines[Index]);
						}
					}
				}
			};

			while (IsProcRunning())
			{
				ReadOutput();
				FPlatformProcess::Sleep(0.2f);
			}
			// The process has exited, so read the remaining output
			ReadOutput();
			if (Output.Len() > 0)
			{
				ProcOutputMsgDelegate.ExecuteIfBound(this, Output + TEXT("\n"));
			}

			int32 ProcReturnCode;
			if (FPlatformProcess::GetProcReturnCode(mProcessHandle, &ProcReturnCode))
			{
				if (ProcReturnCode == 0)
				{
					if (ProcSucceedDelegate.IsBound())
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

	virtual void Exit() override
	{
		Cancel();
	}

	virtual void Cancel() override
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

	virtual uint32 GetProcessId() const { return mProcessID; }
	virtual FProcHandle GetProcessHandle() const { return mProcessHandle; }
	virtual bool IsProcRunning() const { return mProcessHandle.IsValid() && FPlatformProcess::IsApplicationRunning(mProcessID); }

public:
	FProcStatusDelegate ProcBeginDelegate;
	FProcStatusDelegate ProcSucceedDelegate;
	FProcStatusDelegate ProcFailedDelegate;
	FOutputMsgDelegate ProcOutputMsgDelegate;

private:
	FRunnableThread* mThread;
	FString mProgramPath;
	FString mProgramParams;
	FString WorkingDirectory;
	void* mReadPipe;
	void* mWritePipe;
	uint32 mProcessID;
	FProcHandle mProcessHandle;
};
