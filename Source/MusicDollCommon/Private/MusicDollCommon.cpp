#include "MusicDollCommon.h"

DEFINE_LOG_CATEGORY(LogMusicDollCommon);

void FMusicDollCommonModule::StartupModule()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module
}

void FMusicDollCommonModule::ShutdownModule()
{
	// This code will execute after your module is unloaded
}

IMPLEMENT_MODULE(FMusicDollCommonModule, MusicDollCommon);
