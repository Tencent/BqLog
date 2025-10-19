#include "Modules/ModuleManager.h"
#include "Logging/LogMacros.h"

DEFINE_LOG_CATEGORY_STATIC(LogBqLogEditor, Log, All);

class FBqLogEditorModule : public IModuleInterface
{
};

IMPLEMENT_MODULE(FBqLogEditorModule, BqLogEditor)