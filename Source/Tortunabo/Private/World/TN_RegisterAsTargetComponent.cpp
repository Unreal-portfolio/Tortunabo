#include "World/TN_RegisterAsTargetComponent.h"
#include "Core/TN_LevelTargetSubsystem.h"
#include "GameFramework/Actor.h"

UTN_RegisterAsTargetComponent::UTN_RegisterAsTargetComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	bWantsInitializeComponent = false;
}

void UTN_RegisterAsTargetComponent::BeginPlay()
{
	Super::BeginPlay();

	if (TargetTag.IsNone())
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[RegisterAsTarget] '%s' tiene TargetTag=NAME_None — no se registra. Asignar tag en Details."),
			*GetNameSafe(GetOwner()));
		return;
	}

	if (UTN_LevelTargetSubsystem* Sub = UTN_LevelTargetSubsystem::Get(this))
	{
		Sub->RegisterTarget(TargetTag, GetOwner());
	}
}

void UTN_RegisterAsTargetComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (!TargetTag.IsNone())
	{
		if (UTN_LevelTargetSubsystem* Sub = UTN_LevelTargetSubsystem::Get(this))
		{
			Sub->UnregisterTarget(TargetTag);
		}
	}
	Super::EndPlay(EndPlayReason);
}
