#include "Art/TN_Art.h"

#include "Art/TN_ArtCatalog.h"
#include "Art/TN_ArtMeshComponent.h"
#include "Art/TN_ArtSettings.h"
#include "TN_ArtSlots.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Core/TN_Log.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "HAL/IConsoleManager.h"
#include "Materials/MaterialInterface.h"
#include "UObject/GCObject.h"
#include "UObject/StrongObjectPtr.h"
#include "Containers/Ticker.h"

namespace TNArtDetail
{
	TAutoConsoleVariable<int32> CVarArtEnabled(TEXT("TN.Art.Enabled"), 1,
		TEXT("1 = las piezas con sustituto en los catálogos de arte usan su malla final; 0 = se ve todo lo generado (vuelve a cargar el nivel)."));

	/** Catálogos cargados y sustitutos resueltos, retenidos para el recolector. */
	class FCache : public FGCObject
	{
	public:
		bool bCatalogsLoaded = false;
		TArray<TObjectPtr<UTN_ArtCatalog>> Catalogs;
		/** Catálogos de los tests (SetCatalogsForTest): mandan sobre los de los ajustes. */
		TArray<TObjectPtr<UTN_ArtCatalog>> TestCatalogs;
		/** Pieza → sustituto resuelto; un valor nulo = sin sustituto (también se guarda, para no buscarlo cada vez). */
		TMap<FName, TUniquePtr<TNArt::FResolved>> Resolved;
		/** Mallas que no cargaron y ya se avisaron. */
		TSet<FName> Warned;
		/** ISM con sustituto y su ajuste (UpdateInstances). */
		TMap<TWeakObjectPtr<UInstancedStaticMeshComponent>, FTransform> InstanceAdjust;
		/** Piezas anotadas: nombre → veces y lo último que dibujaba. */
		struct FNoted
		{
			int32 Count = 0;
			FString Current;
		};
		TMap<FName, FNoted> Noted;

		virtual void AddReferencedObjects(FReferenceCollector& Collector) override
		{
			Collector.AddReferencedObjects(Catalogs);
			Collector.AddReferencedObjects(TestCatalogs);
			for (TPair<FName, TUniquePtr<TNArt::FResolved>>& Pair : Resolved)
			{
				if (TNArt::FResolved* R = Pair.Value.Get())
				{
					Collector.AddReferencedObject(R->Mesh);
					for (TObjectPtr<UMaterialInterface>& Mat : R->Materials)
					{
						Collector.AddReferencedObject(Mat);
					}
				}
			}
		}

		virtual FString GetReferencerName() const override { return TEXT("TNArt::FCache"); }

		void ClearResolved()
		{
			bCatalogsLoaded = false;
			Catalogs.Reset();
			Resolved.Reset();
			Warned.Reset();
		}
	};

	FCache& Cache()
	{
		static FCache Instance;
		return Instance;
	}

	/** Catálogos en uso (los de los tests si los hay; si no, los de los ajustes, cargados la primera vez). */
	TArray<const UTN_ArtCatalog*> ActiveCatalogs()
	{
		FCache& C = Cache();
		TArray<const UTN_ArtCatalog*> Out;
		if (C.TestCatalogs.Num() > 0)
		{
			for (const TObjectPtr<UTN_ArtCatalog>& Cat : C.TestCatalogs) { Out.Add(Cat.Get()); }
			return Out;
		}
		if (!C.bCatalogsLoaded)
		{
			C.bCatalogsLoaded = true;
			if (const UTN_ArtSettings* Settings = GetDefault<UTN_ArtSettings>())
			{
				for (const TSoftObjectPtr<UTN_ArtCatalog>& Soft : Settings->Catalogs)
				{
					if (Soft.IsNull()) { continue; }
					if (UTN_ArtCatalog* Cat = Soft.LoadSynchronous())
					{
						C.Catalogs.Add(Cat);
					}
					else
					{
						// Sin el asset todavía (Arte aún no lo ha creado): se juega con lo generado, sin ruido.
						UE_LOG(LogTortunabo, Verbose, TEXT("[Arte] No existe el catálogo %s (Ajustes del proyecto > Tortunavy > Arte)."), *Soft.ToString());
					}
				}
			}
		}
		for (const TObjectPtr<UTN_ArtCatalog>& Cat : C.Catalogs) { Out.Add(Cat.Get()); }
		return Out;
	}

	/** Lo que dibuja un componente o una malla, para TN.Art.Slots. */
	FString Describe(const UObject* Obj)
	{
		// Sin malla anotada: la parte de una malla combinada, generada.
		if (!Obj) { return TEXT("generada"); }
		if (const UStaticMesh* Mesh = Cast<UStaticMesh>(Obj))
		{
			// Las generadas cuelgan de su actor (no son assets); las de arte y las del motor, sí.
			return Mesh->IsAsset() ? Mesh->GetPathName() : FString(TEXT("generada"));
		}
		return Obj->GetName();
	}

	/** Etiqueta de los gemelos de colisión de ApplyToInstances. */
	const FName& CollisionTwinTag()
	{
		static const FName Tag(TEXT("TNArtCollision"));
		return Tag;
	}

	/** Se puede tocar: mundo de juego, o componente (o actor) que no se guarda con el nivel. */
	bool CanModify(const UActorComponent* Comp)
	{
		if (!Comp) { return false; }
		const UWorld* World = Comp->GetWorld();
		if (!World) { return false; }
		if (World->IsGameWorld()) { return true; }
		const AActor* Owner = Comp->GetOwner();
		return Comp->HasAnyFlags(RF_Transient) || (Owner && Owner->HasAnyFlags(RF_Transient));
	}

	UTN_ArtMeshComponent* FindArtChild(const USceneComponent* Parent)
	{
		if (!Parent) { return nullptr; }
		for (USceneComponent* Child : Parent->GetAttachChildren())
		{
			UTN_ArtMeshComponent* Art = Cast<UTN_ArtMeshComponent>(Child);
			if (Art && Art->Group.IsNone() && IsValid(Art))
			{
				return Art;
			}
		}
		return nullptr;
	}

	void ApplyMaterials(UMeshComponent* Comp, const TNArt::FResolved& R)
	{
		Comp->EmptyOverrideMaterials();
		for (int32 i = 0; i < R.Materials.Num(); ++i)
		{
			if (R.Materials[i]) { Comp->SetMaterial(i, R.Materials[i]); }
		}
	}

	/** Deja de dibujar el componente generado (sin tocar su visibilidad lógica ni su colisión) y guarda cómo estaba. */
	void HideRender(UStaticMeshComponent* Comp, UTN_ArtMeshComponent::FHiddenState& Out)
	{
		if (!Out.bValid)
		{
			Out.bValid = true;
			Out.bRenderInMainPass = Comp->bRenderInMainPass;
			Out.bRenderInDepthPass = Comp->bRenderInDepthPass;
			Out.bCastShadow = Comp->CastShadow;
			Out.bVisibleInRayTracing = Comp->bVisibleInRayTracing;
			Out.bVisibleInReflectionCaptures = Comp->bVisibleInReflectionCaptures;
			Out.bVisibleInRealTimeSkyCaptures = Comp->bVisibleInRealTimeSkyCaptures;
			Out.bHiddenInSceneCapture = Comp->bHiddenInSceneCapture;
			Out.bAffectDistanceFieldLighting = Comp->bAffectDistanceFieldLighting;
			Out.bAffectDynamicIndirectLighting = Comp->bAffectDynamicIndirectLighting;
			Out.bRenderCustomDepth = Comp->bRenderCustomDepth;
		}
		Comp->bVisibleInReflectionCaptures = false;
		Comp->bVisibleInRealTimeSkyCaptures = false;
		Comp->SetRenderInMainPass(false);
		Comp->SetRenderInDepthPass(false);
		Comp->SetCastShadow(false);
		Comp->SetVisibleInRayTracing(false);
		Comp->SetHiddenInSceneCapture(true);
		Comp->SetAffectDistanceFieldLighting(false);
		Comp->SetAffectDynamicIndirectLighting(false);
		Comp->SetRenderCustomDepth(false);
		Comp->MarkRenderStateDirty();
	}

	void RestoreRender(UStaticMeshComponent* Comp, const UTN_ArtMeshComponent::FHiddenState& In)
	{
		if (!In.bValid) { return; }
		Comp->bVisibleInReflectionCaptures = In.bVisibleInReflectionCaptures;
		Comp->bVisibleInRealTimeSkyCaptures = In.bVisibleInRealTimeSkyCaptures;
		Comp->SetRenderInMainPass(In.bRenderInMainPass);
		Comp->SetRenderInDepthPass(In.bRenderInDepthPass);
		Comp->SetCastShadow(In.bCastShadow);
		Comp->SetVisibleInRayTracing(In.bVisibleInRayTracing);
		Comp->SetHiddenInSceneCapture(In.bHiddenInSceneCapture);
		Comp->SetAffectDistanceFieldLighting(In.bAffectDistanceFieldLighting);
		Comp->SetAffectDynamicIndirectLighting(In.bAffectDynamicIndirectLighting);
		Comp->SetRenderCustomDepth(In.bRenderCustomDepth);
		if (In.bTookCollision) { Comp->SetCollisionEnabled(In.Collision); }
		Comp->MarkRenderStateDirty();
	}

	/** Copia la respuesta de colisión de From (perfil, tipo y respuestas por canal). */
	void CopyCollision(const UPrimitiveComponent* From, UPrimitiveComponent* To)
	{
		To->SetCollisionProfileName(From->GetCollisionProfileName(), false);
		To->SetCollisionObjectType(From->GetCollisionObjectType());
		To->SetCollisionResponseToChannels(From->GetCollisionResponseToChannels());
		To->SetCollisionEnabled(From->GetCollisionEnabled());
		To->CanCharacterStepUpOn = From->CanCharacterStepUpOn;
		To->SetGenerateOverlapEvents(From->GetGenerateOverlapEvents());
		To->SetCanEverAffectNavigation(From->CanEverAffectNavigation());
	}

	void LogSlots(const TArray<FString>& Args)
	{
		FCache& C = Cache();
		const FString Filter = Args.Num() > 0 ? Args[0] : FString();
		int32 Shown = 0, WithArt = 0, Seen = 0;
		UE_LOG(LogTortunabo, Display, TEXT("[Arte] Piezas (TN.Art.Slots [prefijo]): nombre | tipo | vista en esta sesión | lo que se dibuja | sustituto"));
		for (const TNArt::FSlotInfo& Info : TNArt::GetSlotTable())
		{
			const FString Name(Info.Name);
			if (!Filter.IsEmpty() && !Name.StartsWith(Filter)) { continue; }
			const FName Slot(Info.Name);
			const FCache::FNoted* Noted = C.Noted.Find(Slot);
			const TNArt::FResolved* R = TNArt::Find(Slot);
			++Shown;
			Seen += Noted ? 1 : 0;
			WithArt += R ? 1 : 0;
			UE_LOG(LogTortunabo, Display, TEXT("[Arte]   %-44s %-11s %-9s %-36s %s"), Info.Name, Info.Kind,
				Noted ? *FString::Printf(TEXT("x%d"), Noted->Count) : TEXT("no"),
				Noted ? *Noted->Current : TEXT("-"),
				R ? *R->Mesh->GetPathName() : TEXT("(generada)"));
		}
		for (const TPair<FName, FCache::FNoted>& Pair : C.Noted)
		{
			if (!TNArt::FindSlotInfo(Pair.Key))
			{
				UE_LOG(LogTortunabo, Warning, TEXT("[Arte]   %s se usa en el código pero no está en la tabla (Private/Art/TN_ArtSlots_*.inl)."), *Pair.Key.ToString());
			}
		}
		UE_LOG(LogTortunabo, Display, TEXT("[Arte] %d piezas, %d vistas en esta sesión, %d con sustituto. Catálogos: %d."), Shown, Seen, WithArt,
			ActiveCatalogs().Num());
	}

	/** TN.Art.Slots [prefijo] [segundos]: con segundos, la lista sale pasado ese tiempo (lo que se construye al empezar ya está). */
	void HandleSlots(const TArray<FString>& Args)
	{
		TArray<FString> Filter;
		float Delay = 0.f;
		for (const FString& Arg : Args)
		{
			if (Arg.IsNumeric()) { Delay = FCString::Atof(*Arg); }
			else { Filter.Add(Arg); }
		}
		if (Delay <= 0.f)
		{
			LogSlots(Filter);
			return;
		}
		FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Filter](float)
		{
			LogSlots(Filter);
			return false;
		}), Delay);
	}

	FAutoConsoleCommand SlotsCommand(TEXT("TN.Art.Slots"),
		TEXT("Lista las piezas de arte sustituibles (Docs/Arte_Assets.md): tipo, si se han visto en esta sesión, lo que dibujan y su sustituto. Opcional: prefijo (Lobby, ProcMap.Rock...) y segundos de espera."),
		FConsoleCommandWithArgsDelegate::CreateStatic(&HandleSlots));

	FAutoConsoleCommand ReloadCommand(TEXT("TN.Art.Reload"),
		TEXT("Vuelve a leer los catálogos de arte (lo ya construido no cambia hasta volver a cargar el nivel)."),
		FConsoleCommandDelegate::CreateLambda([]()
		{
			TNArt::InvalidateCache();
			UE_LOG(LogTortunabo, Display, TEXT("[Arte] Catálogos recargados: vuelve a cargar el nivel para verlo."));
		}));
}

// ─────────────────────────────────────────────────────────────────────────────
// Catálogo, ajustes y componente de arte
// ─────────────────────────────────────────────────────────────────────────────

#if WITH_EDITOR
void UTN_ArtCatalog::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	TNArt::InvalidateCache();
}
#endif

UTN_ArtSettings::UTN_ArtSettings()
{
	CategoryName = TEXT("Tortunavy");
	SectionName = TEXT("Arte");
}

#if WITH_EDITOR
void UTN_ArtSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	TNArt::InvalidateCache();
}
#endif

UTN_ArtMeshComponent::UTN_ArtMeshComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
	PrimaryComponentTick.bTickEvenWhenPaused = true;
	bTickInEditor = false;
	SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SetCanEverAffectNavigation(false);
	SetGenerateOverlapEvents(false);
	SetMobility(EComponentMobility::Movable);
}

void UTN_ArtMeshComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	// Se ve cuando se ve su padre: el código esconde y enseña la pieza generada como siempre.
	if (const USceneComponent* Parent = GetAttachParent())
	{
		const bool bWant = Parent->IsVisible();
		if (bWant != GetVisibleFlag())
		{
			SetVisibility(bWant);
		}
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// TNArt
// ─────────────────────────────────────────────────────────────────────────────

bool TNArt::IsValidSlotName(const FString& Name)
{
	TArray<FString> Parts;
	Name.ParseIntoArray(Parts, TEXT("."), false);
	if (Parts.Num() < 2) { return false; }
	if (Parts[0] != TEXT("Lobby") && Parts[0] != TEXT("ProcMap") && Parts[0] != TEXT("Beach")) { return false; }
	for (const FString& Part : Parts)
	{
		if (Part.IsEmpty() || !FChar::IsUpper(Part[0])) { return false; }
		for (const TCHAR Ch : Part)
		{
			if (!FChar::IsAlnum(Ch) || Ch > 127) { return false; }
		}
	}
	return true;
}

FString TNArt::ZoneOf(const FString& Name)
{
	int32 Dot = INDEX_NONE;
	return Name.FindChar(TEXT('.'), Dot) ? Name.Left(Dot) : Name;
}

const FTNArtOverride* TNArt::FindIn(TArrayView<const UTN_ArtCatalog* const> Catalogs, FName Slot)
{
	for (const UTN_ArtCatalog* Cat : Catalogs)
	{
		if (!Cat) { continue; }
		const FTNArtOverride* Entry = Cat->Pieces.Find(Slot);
		if (Entry && Entry->HasMesh())
		{
			return Entry;
		}
	}
	return nullptr;
}

const TNArt::FResolved* TNArt::Find(FName Slot)
{
	// Solo en el hilo de juego (la caché no es concurrente): fuera de él, lo generado.
	if (!IsInGameThread() || Slot.IsNone() || TNArtDetail::CVarArtEnabled.GetValueOnGameThread() == 0)
	{
		return nullptr;
	}
	TNArtDetail::FCache& C = TNArtDetail::Cache();
	if (const TUniquePtr<FResolved>* Known = C.Resolved.Find(Slot))
	{
		return Known->Get();
	}
	const TArray<const UTN_ArtCatalog*> Catalogs = TNArtDetail::ActiveCatalogs();
	const FTNArtOverride* Entry = FindIn(Catalogs, Slot);
	TUniquePtr<FResolved> Result;
	if (Entry)
	{
		// Carga síncrona: solo pasa al montar el nivel, la primera vez que se pide la pieza.
		if (UStaticMesh* Mesh = Entry->Mesh.LoadSynchronous())
		{
			Result = MakeUnique<FResolved>();
			Result->Mesh = Mesh;
			Result->Adjust = Entry->Adjust;
			Result->bUseArtCollision = Entry->bUseArtCollision;
			for (const TSoftObjectPtr<UMaterialInterface>& Mat : Entry->Materials)
			{
				Result->Materials.Add(Mat.IsNull() ? nullptr : Mat.LoadSynchronous());
			}
		}
		else if (!C.Warned.Contains(Slot))
		{
			C.Warned.Add(Slot);
			UE_LOG(LogTortunabo, Warning, TEXT("[Arte] %s: la malla %s no carga; se dibuja la generada."), *Slot.ToString(), *Entry->Mesh.ToString());
		}
	}
	const FResolved* Out = Result.Get();
	C.Resolved.Add(Slot, MoveTemp(Result));
	return Out;
}

UStaticMesh* TNArt::Resolve(FName Slot, UStaticMesh* Generated)
{
	const FResolved* R = Find(Slot);
	UStaticMesh* Out = (R && Generated) ? R->Mesh.Get() : Generated;
	NoteSlot(Slot, Out);
	return Out;
}

void TNArt::NoteSlot(FName Slot, const UObject* Current)
{
	if (Slot.IsNone() || !IsInGameThread()) { return; }
	TNArtDetail::FCache::FNoted& Noted = TNArtDetail::Cache().Noted.FindOrAdd(Slot);
	++Noted.Count;
	Noted.Current = TNArtDetail::Describe(Current);
}

void TNArt::SetMesh(UStaticMeshComponent* Comp, UStaticMesh* Generated, FName Slot)
{
	if (!Comp) { return; }
	Comp->SetStaticMesh(Generated);
	ApplyToComponent(Comp, Slot);
}

void TNArt::ApplyToComponent(UStaticMeshComponent* Comp, FName Slot)
{
	using namespace TNArtDetail;
	if (!Comp) { return; }
	UStaticMesh* Generated = Comp->GetStaticMesh();
	const FResolved* R = Generated ? Find(Slot) : nullptr;
	UTN_ArtMeshComponent* Art = FindArtChild(Comp);
	if (!R || !CanModify(Comp))
	{
		// Sin sustituto (o sin pieza): lo generado, como siempre; se deshace lo de una construcción anterior.
		NoteSlot(Slot, Generated);
		if (Art)
		{
			RestoreRender(Comp, Art->Hidden);
			Art->DestroyComponent();
		}
		return;
	}
	// La hija se rehace entera en cada construcción (la colisión de un ISM registrado y con instancias no se cambia): se
	// conserva cómo estaba el componente generado antes de la primera.
	UTN_ArtMeshComponent::FHiddenState Hidden;
	if (Art)
	{
		Hidden = Art->Hidden;
		Art->DestroyComponent();
	}
	if (Hidden.bTookCollision)
	{
		Comp->SetCollisionEnabled(Hidden.Collision);
		Hidden.bTookCollision = false;
	}
	AActor* Owner = Comp->GetOwner();
	Art = NewObject<UTN_ArtMeshComponent>(Owner ? static_cast<UObject*>(Owner) : static_cast<UObject*>(Comp), NAME_None, RF_Transient | RF_DuplicateTransient);
	Art->Slot = Slot;
	Art->Hidden = Hidden;
	Art->SetStaticMesh(R->Mesh);
	ApplyMaterials(Art, *R);
	if (R->bUseArtCollision)
	{
		Art->Hidden.bTookCollision = true;
		Art->Hidden.Collision = Comp->GetCollisionEnabled();
		CopyCollision(Comp, Art);
		Comp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	else
	{
		Art->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	Art->SetupAttachment(Comp);
	Art->SetVisibility(Comp->IsVisible());
	Art->RegisterComponent();
	Art->AddInstance(R->Adjust, false);
	HideRender(Comp, Art->Hidden);
	NoteSlot(Slot, R->Mesh);
}

void TNArt::ApplyToInstances(UInstancedStaticMeshComponent* ISM, FName Slot)
{
	using namespace TNArtDetail;
	if (!ISM) { return; }
	UStaticMesh* Generated = ISM->GetStaticMesh();
	const FResolved* R = Generated ? Find(Slot) : nullptr;
	if (Generated != (R ? R->Mesh.Get() : nullptr))
	{
		// Las instancias están en el espacio de la malla generada: el ajuste de una aplicación anterior ya no vale.
		Cache().InstanceAdjust.Remove(ISM);
	}
	if (!R || !CanModify(ISM) || Generated == R->Mesh)
	{
		NoteSlot(Slot, ISM->GetStaticMesh());
		return;
	}
	// La colisión generada se queda en un gemelo invisible con las mismas instancias.
	if (!R->bUseArtCollision && ISM->GetCollisionEnabled() != ECollisionEnabled::NoCollision)
	{
		UInstancedStaticMeshComponent* Twin = NewObject<UInstancedStaticMeshComponent>(ISM->GetOwner() ? static_cast<UObject*>(ISM->GetOwner()) : static_cast<UObject*>(ISM),
			NAME_None, RF_Transient | RF_DuplicateTransient);
		Twin->ComponentTags.Add(CollisionTwinTag());
		Twin->SetStaticMesh(Generated);
		CopyCollision(ISM, Twin);
		Twin->SetMobility(ISM->Mobility);
		Twin->SetUsingAbsoluteLocation(ISM->IsUsingAbsoluteLocation());
		Twin->SetUsingAbsoluteRotation(ISM->IsUsingAbsoluteRotation());
		Twin->SetUsingAbsoluteScale(ISM->IsUsingAbsoluteScale());
		Twin->SetHiddenInGame(true);
		Twin->SetVisibility(false);
		Twin->SetCastShadow(false);
		if (USceneComponent* Parent = ISM->GetAttachParent())
		{
			Twin->SetupAttachment(Parent, ISM->GetAttachSocketName());
		}
		Twin->SetRelativeTransform(ISM->GetRelativeTransform());
		Twin->RegisterComponent();
		const int32 Num = ISM->GetInstanceCount();
		TArray<FTransform> Xfs;
		Xfs.Reserve(Num);
		for (int32 i = 0; i < Num; ++i)
		{
			FTransform T;
			ISM->GetInstanceTransform(i, T, false);
			Xfs.Add(T);
		}
		Twin->AddInstances(Xfs, false, false);
		ISM->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	ISM->SetStaticMesh(R->Mesh);
	ApplyMaterials(ISM, *R);
	if (!R->Adjust.Equals(FTransform::Identity))
	{
		const int32 Num = ISM->GetInstanceCount();
		TArray<FTransform> Xfs;
		Xfs.Reserve(Num);
		for (int32 i = 0; i < Num; ++i)
		{
			FTransform T;
			ISM->GetInstanceTransform(i, T, false);
			Xfs.Add(R->Adjust * T);
		}
		if (Num > 0) { ISM->BatchUpdateInstancesTransforms(0, Xfs, false, true, true); }
		TMap<TWeakObjectPtr<UInstancedStaticMeshComponent>, FTransform>& Adjusts = Cache().InstanceAdjust;
		for (auto It = Adjusts.CreateIterator(); It; ++It)
		{
			if (!It.Key().IsValid()) { It.RemoveCurrent(); }
		}
		Adjusts.Add(ISM, R->Adjust);
	}
	NoteSlot(Slot, R->Mesh);
}

bool TNArt::UpdateInstances(UInstancedStaticMeshComponent* ISM, int32 StartInstanceIndex, TArrayView<const FTransform> Transforms, bool bWorldSpace,
	bool bMarkRenderStateDirty, bool bTeleport)
{
	if (!ISM) { return false; }
	TNArtDetail::FCache& C = TNArtDetail::Cache();
	const FTransform* Adjust = C.InstanceAdjust.Num() > 0 ? C.InstanceAdjust.Find(ISM) : nullptr;
	if (!Adjust)
	{
		return ISM->BatchUpdateInstancesTransforms(StartInstanceIndex, Transforms, bWorldSpace, bMarkRenderStateDirty, bTeleport);
	}
	static TArray<FTransform> Scratch;
	Scratch.Reset(Transforms.Num());
	for (const FTransform& T : Transforms) { Scratch.Add(*Adjust * T); }
	return ISM->BatchUpdateInstancesTransforms(StartInstanceIndex, Scratch, bWorldSpace, bMarkRenderStateDirty, bTeleport);
}

bool TNArt::CanModify(const UActorComponent* Comp)
{
	return TNArtDetail::CanModify(Comp);
}

bool TNArt::IsCollisionTwin(const UActorComponent* Comp)
{
	return Comp && Comp->ComponentHasTag(TNArtDetail::CollisionTwinTag());
}

void TNArt::InvalidateCache()
{
	TNArtDetail::Cache().ClearResolved();
}

void TNArt::SetCatalogsForTest(const TArray<UTN_ArtCatalog*>& Catalogs)
{
	TNArtDetail::FCache& C = TNArtDetail::Cache();
	C.TestCatalogs.Reset();
	for (UTN_ArtCatalog* Cat : Catalogs) { C.TestCatalogs.Add(Cat); }
	C.Resolved.Reset();
	C.Warned.Reset();
}

TMap<FName, int32> TNArt::GetNotedSlots()
{
	TMap<FName, int32> Out;
	for (const TPair<FName, TNArtDetail::FCache::FNoted>& Pair : TNArtDetail::Cache().Noted)
	{
		Out.Add(Pair.Key, Pair.Value.Count);
	}
	return Out;
}
