#include "Modules/ModuleManager.h"
#include "Logging/LogMacros.h"

DEFINE_LOG_CATEGORY_STATIC(LogBqLogNodesModule, Log, All);

class FBqLogBPNodesModule : public IModuleInterface
{
};

IMPLEMENT_MODULE(FBqLogBPNodesModule, BqLogBPNodes)