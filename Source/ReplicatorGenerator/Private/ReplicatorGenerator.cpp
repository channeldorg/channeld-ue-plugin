#include "ReplicatorGenerator.h"

#define LOCTEXT_NAMESPACE "FReplicatorGeneratorModule"

void FReplicatorGeneratorModule::StartupModule()
{
#if WITH_EDITOR && ENGINE_MAJOR_VERSION >= 5
    FString CommandLineArgs = FCommandLine::Get();
    if (CommandLineArgs.Contains(TEXT("CookAndFilterRepActor"), ESearchCase::IgnoreCase) || 
        CommandLineArgs.Contains(TEXT("CookAndUpdateRepActorCache"), ESearchCase::IgnoreCase))
    {
        PRIVATE_GIsRunningCookCommandlet = true;
    }
#endif
}

void FReplicatorGeneratorModule::ShutdownModule()
{
    
}

#undef LOCTEXT_NAMESPACE
    
IMPLEMENT_MODULE(FReplicatorGeneratorModule, ReplicatorGenerator)