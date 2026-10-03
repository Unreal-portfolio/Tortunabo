#include "World/Beach/TN_BeachTickWakeSubsystem.h"
#include "World/Beach/TN_BeachElement.h"
#include "Player/TN_ShellBody.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"

namespace
{
	TAutoConsoleVariable<int32> CVarBeachTickWake(TEXT("TN.Perf.BeachTickWake"), 1,
		TEXT("1: los elementos de la playa que lo piden apagan su Tick sin nadie cerca (#59). 0: todos despiertos, como antes."));
}

bool UTN_BeachTickWakeSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return World && World->IsGameWorld() && Super::ShouldCreateSubsystem(Outer);
}

void UTN_BeachTickWakeSubsystem::GatherWatchers(TArray<FVector>& OutWatchers) const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	// Tortugas (y cualquier personaje) y caparazones de todas: los sensores del servidor y lo que se ve en cada cliente.
	for (TActorIterator<ACharacter> It(World); It; ++It)
	{
		OutWatchers.Add(It->GetActorLocation());
	}
	for (TActorIterator<ATN_ShellBody> It(World); It; ++It)
	{
		OutWatchers.Add(It->GetActorLocation());
	}
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		const APlayerController* PC = It->Get();
		if (PC && PC->IsLocalController() && PC->PlayerCameraManager)
		{
			OutWatchers.Add(PC->PlayerCameraManager->GetCameraLocation());
		}
	}
}

void UTN_BeachTickWakeSubsystem::Evaluate(ATN_BeachElement& Element, TConstArrayView<FVector> Watchers, bool bEnabled)
{
	const bool bWasAwake = Element.IsActorTickEnabled();
	const bool bAwake = !bEnabled || TNBeachTickWake::ShouldBeAwake(
		TNBeachTickWake::NearestDistSquared(Element.GetActorLocation(), Watchers), Element.GetTickWakeDistance(), bWasAwake,
		Element.IsTickBusy());
	if (bAwake != bWasAwake)
	{
		Element.SetActorTickEnabled(bAwake);
		Element.OnTickWakeChanged(bAwake);
	}
}

void UTN_BeachTickWakeSubsystem::Register(ATN_BeachElement* Element)
{
	if (!Element)
	{
		return;
	}
	Elements.AddUnique(Element);
	TArray<FVector> Watchers;
	GatherWatchers(Watchers);
	Evaluate(*Element, Watchers, CVarBeachTickWake.GetValueOnGameThread() != 0);
}

void UTN_BeachTickWakeSubsystem::Unregister(ATN_BeachElement* Element)
{
	Elements.RemoveAllSwap([Element](const TWeakObjectPtr<ATN_BeachElement>& Entry) { return !Entry.IsValid() || Entry.Get() == Element; });
}

void UTN_BeachTickWakeSubsystem::Tick(float DeltaTime)
{
	Clock -= DeltaTime;
	if (Clock > 0.f)
	{
		return;
	}
	Clock = TNBeachTickWake::CheckInterval;

	TArray<FVector> Watchers;
	GatherWatchers(Watchers);
	const bool bEnabled = CVarBeachTickWake.GetValueOnGameThread() != 0;
	for (int32 Index = Elements.Num() - 1; Index >= 0; --Index)
	{
		ATN_BeachElement* Element = Elements[Index].Get();
		if (!Element)
		{
			Elements.RemoveAtSwap(Index);
			continue;
		}
		Evaluate(*Element, Watchers, bEnabled);
	}
}
