#include "Testing/TN_TestReport.h"

#include "Dom/JsonObject.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMemory.h"
#include "Misc/App.h"
#include "Misc/Parse.h"
#include "Misc/CommandLine.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Policies/PrettyJsonPrintPolicy.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

FString TNTestReport::DefaultPath(const TCHAR* Folder)
{
	return FPaths::Combine(FPaths::ProjectSavedDir(), Folder, FDateTime::Now().ToString(TEXT("%Y-%m-%d_%H-%M-%S")) + TEXT(".json"));
}

bool TNTestReport::Save(const FJsonObject& Object, const FString& Path)
{
	FString Text;
	const TSharedRef<TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>> Writer = TJsonWriterFactory<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>::Create(&Text);
	// El serializador pide un TSharedRef<FJsonObject>: se copia el contenido a uno nuevo sin tocar el original.
	TSharedRef<FJsonObject> Copy = MakeShared<FJsonObject>();
	Copy->Values = Object.Values;
	if (!FJsonSerializer::Serialize(Copy, Writer))
	{
		return false;
	}
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path), true);
	return FFileHelper::SaveStringToFile(Text, *Path, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
}

FString TNTestReport::CommandLineValue(const TCHAR* Key)
{
	FString Value;
	FParse::Value(FCommandLine::Get(), *(FString(Key) + TEXT("=")), Value);
	return Value;
}

FString TNTestReport::BuildConfigName()
{
	return LexToString(FApp::GetBuildConfiguration());
}

FString TNTestReport::NetModeName(const UWorld* World)
{
	if (!World)
	{
		return TEXT("?");
	}
	switch (World->GetNetMode())
	{
		case NM_Standalone:       return TEXT("Standalone");
		case NM_DedicatedServer:  return TEXT("DedicatedServer");
		case NM_ListenServer:     return TEXT("ListenServer");
		case NM_Client:           return TEXT("Client");
		default:                  return TEXT("?");
	}
}

double TNTestReport::UsedPhysicalMB()
{
	return static_cast<double>(FPlatformMemory::GetStats().UsedPhysical) / (1024.0 * 1024.0);
}

double TNTestReport::PeakPhysicalMB()
{
	return static_cast<double>(FPlatformMemory::GetStats().PeakUsedPhysical) / (1024.0 * 1024.0);
}
