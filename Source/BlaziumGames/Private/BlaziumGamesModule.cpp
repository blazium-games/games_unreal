#include "BlaziumGamesModule.h"

#include "BlaziumApi.h"

IMPLEMENT_MODULE(FBlaziumGamesModule, BlaziumGames)

void FBlaziumGamesModule::StartupModule()
{
}

void FBlaziumGamesModule::ShutdownModule()
{
	FBlaziumApi::Get().Release();
}
