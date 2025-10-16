#include "Modules/ModuleManager.h"
#include "Logging/LogMacros.h"
DEFINE_LOG_CATEGORY_STATIC(LogBqLog, Log, All);
class FBqLogModule : public IModuleInterface
{
public:
    virtual void StartupModule() override { UE_LOG(LogBqLog, Log, TEXT("BqLog started")); }
    virtual void ShutdownModule() override {}
};
IMPLEMENT_MODULE(FBqLogModule, BqLog)