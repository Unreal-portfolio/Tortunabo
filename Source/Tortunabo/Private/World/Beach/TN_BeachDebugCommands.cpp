#include "World/Beach/TN_BeachElement.h"
#include "World/Beach/TN_BeachTypes.h"
#include "Core/TN_Log.h"
#include "CollisionQueryParams.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"

/**
 * Consola de prueba de los elementos de la playa (cualquiera de ETNBeachElement: decorado, trampas o enemigos), en
 * cualquier mapa:
 *
 *   TN.Beach.Place <Elemento|número> [Tamaño=1] [Extent=0] [Semilla=al azar]
 *   TN.Beach.Place clear
 *
 * Lo crea el servidor (ATN_BeachElement::SpawnElement, replicado) delante de la tortuga local, apoyado en el suelo y
 * mirando hacia donde mira ella (su X local = la dirección de la carrera). Desde la ventana de un cliente del PIE se crea
 * en el mundo del servidor del mismo proceso; en un cliente remoto de verdad no hace nada (hay que escribirlo en el
 * anfitrión). «clear» borra todo lo creado así (etiqueta TNBeachDebug).
 *
 * TN.Beach.Spawn (TN_BeachEnemyDebug.cpp) hace lo mismo más simple: siempre a 30 m, con tamaño 1 y sin apoyarlo en el
 * suelo. Este se llama distinto para no registrar dos veces el mismo comando.
 */
namespace TNBeachDebugCommands
{
	const FName DebugTag(TEXT("TNBeachDebug"));

	/** El mundo con autoridad del mismo proceso (el propio si no es un cliente; en PIE, el del servidor del mismo mapa). */
	UWorld* FindAuthorityWorld(UWorld* InWorld)
	{
		if (!InWorld)
		{
			return nullptr;
		}
		if (InWorld->GetNetMode() != NM_Client)
		{
			return InWorld;
		}
		if (!GEngine)
		{
			return nullptr;
		}
		const FString MapName = UWorld::RemovePIEPrefix(InWorld->GetMapName());
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			UWorld* Candidate = Context.World();
			if (Candidate && Candidate != InWorld && Candidate->IsGameWorld() && Candidate->GetNetMode() != NM_Client
				&& UWorld::RemovePIEPrefix(Candidate->GetMapName()) == MapName)
			{
				return Candidate;
			}
		}
		return nullptr;
	}

	bool ParseElement(const FString& Arg, ETNBeachElement& OutElement)
	{
		if (Arg.IsNumeric())
		{
			const int32 Value = FCString::Atoi(*Arg);
			if (Value >= 0 && Value < static_cast<int32>(ETNBeachElement::Count))
			{
				OutElement = static_cast<ETNBeachElement>(Value);
				return true;
			}
			return false;
		}
		const UEnum* Enum = StaticEnum<ETNBeachElement>();
		if (!Enum)
		{
			return false;
		}
		const int64 Value = Enum->GetValueByNameString(Arg);
		if (Value == INDEX_NONE || Value >= static_cast<int64>(ETNBeachElement::Count))
		{
			return false;
		}
		OutElement = static_cast<ETNBeachElement>(Value);
		return true;
	}

	/** Altura del suelo bajo XY (sin personajes), o Fallback si no hay nada debajo. */
	double GroundZ(UWorld* World, const FVector& At, double Fallback)
	{
		FHitResult Hit;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(TNBeachDebugGround), false);
		FCollisionObjectQueryParams Objects;
		Objects.AddObjectTypesToQuery(ECC_WorldStatic);
		Objects.AddObjectTypesToQuery(ECC_WorldDynamic);
		if (World->LineTraceSingleByObjectType(Hit, At + FVector(0.0, 0.0, 3000.0), At - FVector(0.0, 0.0, 6000.0), Objects, Params))
		{
			return Hit.ImpactPoint.Z;
		}
		return Fallback;
	}

	void RunSpawn(const TArray<FString>& Args, UWorld* InWorld)
	{
		UWorld* AuthWorld = FindAuthorityWorld(InWorld);
		if (!AuthWorld)
		{
			UE_LOG(LogTortunabo, Warning, TEXT("[Playa] TN.Beach.Place: solo en el servidor (escríbelo en la ventana del anfitrión)."));
			return;
		}
		if (Args.Num() > 0 && Args[0].Equals(TEXT("clear"), ESearchCase::IgnoreCase))
		{
			int32 Removed = 0;
			for (TActorIterator<ATN_BeachElement> It(AuthWorld); It; ++It)
			{
				if (It->ActorHasTag(DebugTag))
				{
					It->Destroy();
					++Removed;
				}
			}
			UE_LOG(LogTortunabo, Log, TEXT("[Playa] TN.Beach.Place clear: %d elementos borrados."), Removed);
			return;
		}
		ETNBeachElement Element = ETNBeachElement::Coconut;
		if (Args.Num() < 1 || !ParseElement(Args[0], Element))
		{
			UE_LOG(LogTortunabo, Display, TEXT("[Playa] Uso: TN.Beach.Place <Elemento|número> [Tamaño=1] [Extent=0] [Semilla] | TN.Beach.Place clear. Elementos: los de ETNBeachElement (Coconut ... GullZone; p. ej. Seaweed, SandDungeon)."));
			return;
		}
		const float Size = Args.Num() > 1 ? FMath::Clamp(FCString::Atof(*Args[1]), 0.3f, 2.f) : 1.f;
		const float Extent = Args.Num() > 2 ? FMath::Max(0.f, FCString::Atof(*Args[2])) : 0.f;
		const int32 Seed = Args.Num() > 3 ? FCString::Atoi(*Args[3]) : FMath::Rand();

		// Delante de la tortuga local de la ventana donde se escribe (la misma posición en el mundo del servidor).
		const APlayerController* PC = InWorld->GetFirstPlayerController();
		const APawn* Viewer = PC ? PC->GetPawn() : nullptr;
		FVector From = FVector::ZeroVector;
		FRotator Facing = FRotator::ZeroRotator;
		if (Viewer)
		{
			From = Viewer->GetActorLocation();
			Facing = FRotator(0.0, Viewer->GetActorRotation().Yaw, 0.0);
		}
		else if (PC)
		{
			FVector ViewLoc;
			FRotator ViewRot;
			PC->GetPlayerViewPoint(ViewLoc, ViewRot);
			From = ViewLoc;
			Facing = FRotator(0.0, ViewRot.Yaw, 0.0);
		}
		const double Radius = TNBeach::FootprintRadius(Element) * Size;
		FVector At = From + Facing.Vector() * (Radius + 400.0);
		At.Z = GroundZ(AuthWorld, At, From.Z - 90.0);

		FTNBeachElementSpec ElementSpec;
		ElementSpec.Element = Element;
		ElementSpec.Seed = Seed;
		ElementSpec.SizeScale = Size;
		ElementSpec.Extent = Extent;
		ATN_BeachElement* Spawned = ATN_BeachElement::SpawnElement(AuthWorld, FTransform(Facing, At), ElementSpec);
		if (!Spawned)
		{
			UE_LOG(LogTortunabo, Warning, TEXT("[Playa] TN.Beach.Place: no se ha podido crear %s (¿aún no existe su clase %s?)."),
				*UEnum::GetValueAsString(Element), TNBeach::ClassNameOf(Element));
			return;
		}
		Spawned->Tags.AddUnique(DebugTag);
		UE_LOG(LogTortunabo, Log, TEXT("[Playa] TN.Beach.Place: %s (%s) en %s, tamaño %.2f, extent %.0f, semilla %d."), *UEnum::GetValueAsString(Element),
			*Spawned->GetName(), *At.ToString(), Size, Extent, Seed);
	}

	static FAutoConsoleCommandWithWorldAndArgs SpawnCommand(
		TEXT("TN.Beach.Place"),
		TEXT("Crea un elemento de la playa delante de la tortuga local: TN.Beach.Place <Elemento|número> [Tamaño=1] [Extent=0] [Semilla]. «TN.Beach.Place clear» borra los creados así."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&RunSpawn),
		ECVF_Cheat);
}
