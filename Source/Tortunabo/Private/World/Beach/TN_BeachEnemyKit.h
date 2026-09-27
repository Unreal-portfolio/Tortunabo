#pragma once

#include "CoreMinimal.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInterface.h"
#include "UObject/Package.h"
#include "World/ProcMap/TN_ProcMapAmbientFX.h"
#include "World/ProcMap/TN_ProcMapMeshKit.h"
#include "World/ProcMap/TN_ProcMapRuntimeMesh.h"

/**
 * Utilidades de los enemigos de la playa (solo visual): materiales de color de vértice, mallas en ejecución guardadas
 * por nombre (se comparten entre actores y el recolector las libera cuando nadie las usa), piezas animables, emisores de
 * partículas propios (TNAmbientFX, sin el registro global) y la sombra redonda que se pinta en la arena.
 */
namespace TNBeachKit
{
	using TNProcMesh::FTNProcMeshBuffers;

	/** Material opaco de color de vértice (el de la vegetación y la fauna; si no está, el de los cosméticos o el del motor). */
	inline UMaterialInterface* SolidMaterial()
	{
		static TWeakObjectPtr<UMaterialInterface> Cached;
		if (!Cached.IsValid())
		{
			UMaterialInterface* Mat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/ProcMap/Materials/M_ProcFoliage.M_ProcFoliage"), nullptr, LOAD_NoWarn);
			if (!Mat)
			{
				Mat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Cosmetics/Materials/M_CosmeticVertexColor.M_CosmeticVertexColor"), nullptr, LOAD_NoWarn);
			}
			if (!Mat)
			{
				Mat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/EngineDebugMaterials/VertexColorMaterial.VertexColorMaterial"));
			}
			Cached = Mat;
		}
		return Cached.Get();
	}

	/** Material translúcido sin luz (alfa del vértice = opacidad): sombras, velos y efectos. */
	inline UMaterialInterface* SoftMaterial()
	{
		static TWeakObjectPtr<UMaterialInterface> Cached;
		if (!Cached.IsValid())
		{
			UMaterialInterface* Mat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/ProcMap/Materials/M_ProcFXSoft.M_ProcFXSoft"), nullptr, LOAD_NoWarn);
			Cached = Mat ? Mat : SolidMaterial();
		}
		return Cached.Get();
	}

	/** Cómo se guarda el alfa del vértice de una malla. */
	enum class EBeachMeshMat : uint8
	{
		/** Opaca (alfa 0: sin balanceo de viento). */
		Solid,
		/** Translúcida con el alfa de los propios buffers. */
		SoftVertexAlpha
	};

	/**
	 * Malla estática en ejecución guardada por nombre: la primera vez se construye con Build; después se reutiliza
	 * mientras algún componente la use (referencia débil: si nadie la usa, el recolector la libera y se rehace).
	 */
	inline UStaticMesh* CachedMesh(const FString& Key, TFunctionRef<void(FTNProcMeshBuffers&)> Build, EBeachMeshMat Mat = EBeachMeshMat::Solid)
	{
		static TMap<FString, TWeakObjectPtr<UStaticMesh>> Cache;
		if (const TWeakObjectPtr<UStaticMesh>* Found = Cache.Find(Key))
		{
			if (UStaticMesh* Existing = Found->Get())
			{
				return Existing;
			}
		}
		FTNProcMeshBuffers B;
		Build(B);
		const bool bSoft = Mat == EBeachMeshMat::SoftVertexAlpha;
		UStaticMesh* Mesh = TNProcRuntimeMesh::MakeStaticMesh(GetTransientPackage(), B, bSoft ? SoftMaterial() : SolidMaterial(), false, 0.f, 1.f, bSoft ? -2.f : -1.f);
		Cache.Add(Key, Mesh);
		return Mesh;
	}

	/** Pieza de malla enganchada a Parent en RelLoc, sin colisión (móvil, sombra opcional). */
	inline UStaticMeshComponent* AddPart(AActor* Owner, USceneComponent* Parent, UStaticMesh* Mesh, const FVector& RelLoc, bool bShadow = true)
	{
		if (!Owner || !Parent)
		{
			return nullptr;
		}
		UStaticMeshComponent* Comp = NewObject<UStaticMeshComponent>(Owner, NAME_None, RF_Transient);
		Comp->SetMobility(EComponentMobility::Movable);
		Comp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Comp->SetCanEverAffectNavigation(false);
		Comp->SetGenerateOverlapEvents(false);
		Comp->SetCastShadow(bShadow);
		Comp->bReceivesDecals = false;
		Comp->SetStaticMesh(Mesh);
		Comp->SetupAttachment(Parent);
		Comp->SetRelativeLocation(RelLoc);
		Comp->RegisterComponent();
		return Comp;
	}

	/** Coloca la pieza en su pivote con un giro y una escala (evita tocar el componente si no está). */
	inline void Pose(UStaticMeshComponent* Comp, const FVector& Loc, const FRotator& Rot, const FVector& Scale = FVector::OneVector)
	{
		if (Comp)
		{
			Comp->SetRelativeTransform(FTransform(Rot, Loc, Scale));
		}
	}

	/** Añade el triángulo y escribe el alfa de sus tres vértices con AlphaOf(posición). */
	template <typename TAlphaFn>
	inline void AddTriAlpha(FTNProcMeshBuffers& M, const FVector& A, const FVector& B, const FVector& C, const FVector& Hint, const FLinearColor& Color, TAlphaFn AlphaOf)
	{
		const int32 Before = M.Verts.Num();
		M.AddTri(A, B, C, Hint, Color);
		for (int32 i = Before; i < M.Verts.Num(); ++i)
		{
			M.Colors[i].A = AlphaOf(M.Verts[i]);
		}
	}

	/**
	 * Sombra redonda de radio 100 cm mirando arriba: casi negra y con el borde difuminado (alfa del vértice). Se escala en
	 * cada uso; Opacity va en el nombre de la caché.
	 */
	inline UStaticMesh* ShadowDisc(float Opacity = 0.45f)
	{
		const FString Key = FString::Printf(TEXT("Beach.Shadow.%d"), FMath::RoundToInt32(Opacity * 100.f));
		return CachedMesh(Key, [Opacity](FTNProcMeshBuffers& M)
		{
			constexpr int32 Seg = 24;
			const FLinearColor Dark(0.02f, 0.02f, 0.03f, 1.f);
			auto AlphaOf = [Opacity](const FVector& P)
			{
				const double R = P.Size2D() / 100.0;
				return R < 0.55 ? Opacity : static_cast<float>(Opacity * FMath::Clamp((1.0 - R) / 0.45, 0.0, 1.0));
			};
			for (int32 k = 0; k < Seg; ++k)
			{
				const double A0 = TNProcMap::TwoPi * k / Seg;
				const double A1 = TNProcMap::TwoPi * (k + 1) / Seg;
				const FVector I0(FMath::Cos(A0) * 55.0, FMath::Sin(A0) * 55.0, 0.0);
				const FVector I1(FMath::Cos(A1) * 55.0, FMath::Sin(A1) * 55.0, 0.0);
				const FVector O0(FMath::Cos(A0) * 100.0, FMath::Sin(A0) * 100.0, 0.0);
				const FVector O1(FMath::Cos(A1) * 100.0, FMath::Sin(A1) * 100.0, 0.0);
				AddTriAlpha(M, FVector::ZeroVector, I0, I1, FVector::UpVector, Dark, AlphaOf);
				AddTriAlpha(M, I0, O0, O1, FVector::UpVector, Dark, AlphaOf);
				AddTriAlpha(M, I0, O1, I1, FVector::UpVector, Dark, AlphaOf);
			}
		}, EBeachMeshMat::SoftVertexAlpha);
	}

	/** Pieza de sombra en el suelo (sin sombra propia, sin colisión, absoluta: se coloca en el mundo). */
	inline UStaticMeshComponent* AddShadow(AActor* Owner, float Opacity = 0.45f)
	{
		UStaticMeshComponent* Comp = AddPart(Owner, Owner ? Owner->GetRootComponent() : nullptr, ShadowDisc(Opacity), FVector::ZeroVector, false);
		if (Comp)
		{
			Comp->SetAbsolute(true, true, true);
			Comp->SetTranslucentSortPriority(2);
		}
		return Comp;
	}

	/** Coloca una sombra en el mundo: en Ground (un pelo por encima), con radio Radius (cm). Radius <= 0 la esconde. */
	inline void PlaceShadow(UStaticMeshComponent* Comp, const FVector& Ground, float Radius)
	{
		if (!Comp)
		{
			return;
		}
		const bool bShow = Radius > 1.f;
		if (Comp->IsVisible() != bShow)
		{
			Comp->SetVisibility(bShow);
		}
		if (bShow)
		{
			const double S = Radius / 100.0;
			Comp->SetWorldTransform(FTransform(FQuat::Identity, Ground + FVector(0.0, 0.0, 12.0), FVector(S, S, 1.0)));
		}
	}

	/** Emisor de partículas propio del actor (no pasa por el registro global de TNAmbientFX). Nada en servidor dedicado. */
	inline void InitEmitter(TNAmbientFX::FEmitter& E, AActor* Owner, const TNAmbientFX::FEmitterDesc& Desc, uint32 Seed)
	{
		E = TNAmbientFX::FEmitter();
		if (!Owner || !Owner->GetWorld() || Owner->GetWorld()->GetNetMode() == NM_DedicatedServer)
		{
			return;
		}
		E.Desc = Desc;
		E.RateScale = 0.f;
		E.Rng ^= Seed * 2654435761u + 1u;
		E.Particles.SetNum(Desc.MaxParticles);
		E.Xf.Init(FTransform(FQuat::Identity, FVector::ZeroVector, FVector::ZeroVector), Desc.MaxParticles);
		E.ISM = TNAmbientFX::MakeISM(Owner, TNAmbientFX::ShapeMesh(Desc.Shape, Desc.Color, Desc.bSoft, Desc.Alpha, Desc.bCloud), Desc.MaxParticles, false);
	}

	/** true si al emisor le queda alguna partícula viva. */
	inline bool AnyAlive(const TNAmbientFX::FEmitter& E)
	{
		for (const TNAmbientFX::FParticle& P : E.Particles)
		{
			if (P.bAlive)
			{
				return true;
			}
		}
		return false;
	}

	/** Mueve el emisor si nace algo o le queda algo vivo (si no, no cuesta nada). */
	inline void TickEmitterIfBusy(TNAmbientFX::FEmitter& E, float Dt, const FVector& View)
	{
		if (!E.ISM.IsValid())
		{
			return;
		}
		if (E.RateScale > 0.f || E.bAwake || AnyAlive(E))
		{
			TNAmbientFX::TickEmitter(E, Dt, View);
			if (E.RateScale <= 0.f && !AnyAlive(E))
			{
				E.bAwake = false;
			}
		}
	}

	/** Estallido de Count partículas desde Origin hacia Dir. */
	inline void BurstAt(TNAmbientFX::FEmitter& E, const FVector& Origin, const FVector& Dir, int32 Count)
	{
		if (!E.ISM.IsValid())
		{
			return;
		}
		E.Origin = Origin;
		E.Desc.Direction = Dir;
		TNAmbientFX::Burst(E, Count);
	}

	/** Descripción de un emisor con lo habitual rellenado. */
	inline TNAmbientFX::FEmitterDesc MakeDesc(TNAmbientFX::EShape Shape, const FLinearColor& Color, bool bSoft, float Alpha, int32 MaxParticles,
		float Rate, float Speed, float Gravity, float LifeMin, float LifeMax, float SizeStart, float SizeEnd)
	{
		TNAmbientFX::FEmitterDesc D;
		D.Shape = Shape;
		D.Color = Color;
		D.bSoft = bSoft;
		D.bCloud = bSoft && Shape == TNAmbientFX::EShape::Puff;
		D.Alpha = Alpha;
		D.MaxParticles = MaxParticles;
		D.Rate = Rate;
		D.Speed = Speed;
		D.SpeedJitter = 0.4f;
		D.Spread = 0.6f;
		D.Gravity = Gravity;
		D.Drag = 0.6f;
		D.LifeMin = LifeMin;
		D.LifeMax = LifeMax;
		D.SizeStart = SizeStart;
		D.SizeEnd = SizeEnd;
		D.WakeDistance = 60000.f;
		return D;
	}

	/** Cámara local (la primera): posición. Falso sin jugador local. */
	inline bool LocalCamera(const UWorld* World, FVector& OutLoc)
	{
		const APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
		if (!PC || !PC->PlayerCameraManager)
		{
			return false;
		}
		OutLoc = PC->PlayerCameraManager->GetCameraLocation();
		return true;
	}

	/** Hash estable de un entero a [0, 1). */
	inline float Hash01(uint32 X)
	{
		X ^= X >> 16;
		X *= 0x7feb352du;
		X ^= X >> 15;
		X *= 0x846ca68bu;
		X ^= X >> 16;
		return static_cast<float>(X & 0xFFFFFF) / 16777216.f;
	}
}
