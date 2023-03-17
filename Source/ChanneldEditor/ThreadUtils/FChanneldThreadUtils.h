#pragma once
#include "HAL/Runnable.h"
#include "HAL/RunnableThread.h"
DECLARE_MULTICAST_DELEGATE(FChanneldThreadWorkerStatusDelegate);

namespace EChanneldThreadStatus
{
	enum Type
	{
		InActive,

		Busy,

		Canceling,

		Canceled,

		Completed
	};

}
class FChanneldThreadWorker : public FRunnable
{
public:
	using FCallback = TFunction<void()>;
	explicit FChanneldThreadWorker(const TCHAR *InThreadName, const FCallback& InRunFunc)
		:mThreadName(InThreadName),mRunFunc(InRunFunc),mThreadStatus(EChanneldThreadStatus::InActive)
	{}

	virtual void Execute()
	{
		if (GetThreadStatus() == EChanneldThreadStatus::InActive)
		{
			mThread = FRunnableThread::Create(this, *mThreadName);
			if (mThread)
			{
				mThreadStatus = EChanneldThreadStatus::Busy;
			}
		}
	}
	virtual void Join()
	{
		mThread->WaitForCompletion();
	}

	virtual uint32 Run()override
	{
		mRunFunc();

		return 0;
	}
	virtual void Stop()override
	{
		Cancel();
	}
	virtual void Cancel()
	{
		mThreadStatus = EChanneldThreadStatus::Canceled;
	}
	virtual void Exit()override
	{
		mThreadStatus = EChanneldThreadStatus::Completed;
	}

	virtual EChanneldThreadStatus::Type GetThreadStatus()const
	{
		return mThreadStatus;
	}
public:
	FChanneldThreadWorkerStatusDelegate CancelDelegate;
	FORCEINLINE FString GetThreadName()const {return mThreadName;}
protected:
	FString mThreadName;
	FCallback mRunFunc;
	FRunnableThread* mThread;
	volatile EChanneldThreadStatus::Type mThreadStatus;

private:
	FChanneldThreadWorker(const FChanneldThreadWorker&) = delete;
	FChanneldThreadWorker& operator=(const FChanneldThreadWorker&) = delete;

};
