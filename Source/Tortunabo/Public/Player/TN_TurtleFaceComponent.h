#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Core/TN_MatchFlowTypes.h"
#include "TN_TurtleFaceComponent.generated.h"

class ATN_CoopGameState;
class ATortugaCharacter;
class UMaterialInstanceDynamic;
class UProceduralMeshComponent;
class UProximityVoiceComponent;
class USceneComponent;
class USkeletalMeshComponent;

/** Lo que la cara quiere hacer en un fotograma, antes de suavizar (definido en TN_TurtleFaceComponent.cpp). */
struct FTNTurtleFaceGoal;

/** Ánimo de la cara 3D, con los mismos umbrales de energía que las caras del HUD. */
enum class ETNTurtleFaceMood : uint8
{
	Happy,
	Tired,
	Panting,
	Down,
	Shell,
};

/**
 * Cara de la tortuga del jugador: lengua con física, caras de cansancio, sudor y boca que habla. Todo es cosmético y
 * local en cada máquina, calculado con estado que ya se replica (estamina, sprint, derribo, caparazón, emote, chat
 * rápido y voz): no manda nada por la red.
 *
 * - Lengua: la de la malla (TotugaDemo_Rig, ranura "lambert2") no tiene hueso y se oculta en M_TurtleHelmetSlot; esta
 *   la sustituye. Es una malla procedural (rosa con su surco) sobre una cadena de puntos simulada aquí (muelles de forma
 *   que la mantienen curvada, gravedad, rozamiento del aire y aleteo), enganchada al hueso Head dentro del hueco de la
 *   boca. En reposo está dentro; al esprintar sale por un lado y aletea al viento como un perro asomado a la ventanilla
 *   (en las curvas se va al lado de fuera); al jadear cuelga por delante; noqueada, cae floja por un lado; y de vez en
 *   cuando, quieta y contenta, asoma la punta.
 * - Cara (parámetros de M_TurtleBody): EyeTired (párpados a media asta y mirada baja), EyeSqueeze («>_<» al agotarse y
 *   al gritar), MouthOpen y MouthSmile (la boca pintada alrededor del hueco de la malla) y FaceBlush (colorete), como
 *   las caras del HUD (TN_HUDFaces.h): feliz, cansada y jadeando. Con cansancio salen gotas de sudor junto al casco.
 * - Hablar: al llegar un mensaje del chat rápido de su jugador (a la vez que el bocadillo del HUD), la boca se abre y
 *   se cierra por sílabas un rato según lo largo del mensaje; también mientras se le oye por la voz de proximidad.
 *
 * Solo actúa con la malla de demo (la de dos ranuras con M_TurtleBody). Pruebas: tn.Face.Mood, tn.Face.Tongue y
 * tn.Face.Talk (ver Docs/Animacion_Tortuga.md).
 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class TORTUNABO_API UTN_TurtleFaceComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UTN_TurtleFaceComponent();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** Mueve la boca como si hablara durante Seconds (el chat rápido lo llama solo). Local. */
	UFUNCTION(BlueprintCallable, Category = "Face")
	void TalkFor(float Seconds);

	/** Largo de la lengua al viento, fuera de la boca (unidades de la malla; 2,5 cm cada una en el personaje). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Face|Tongue", meta = (ClampMin = "1.0"))
	float SprintTongueLength = 8.f;

	/** Largo de la lengua colgando al jadear (unidades de la malla). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Face|Tongue", meta = (ClampMin = "1.0"))
	float PantTongueLength = 6.5f;

	/** Medio ancho de la lengua (unidades de la malla; el hueco de la boca mide unas 1,7 de medio ancho). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Face|Tongue", meta = (ClampMin = "0.2"))
	float TongueHalfWidth = 1.3f;

	/** Rigidez de la forma de la lengua al viento (1/s²): más baja, más se dobla hacia atrás y más aletea. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Face|Tongue", meta = (ClampMin = "10.0"))
	float TongueShapeStiffness = 900.f;

	/** Gravedad sobre la lengua (fracción de la real). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Face|Tongue", meta = (ClampMin = "0.0"))
	float TongueGravity = 0.6f;

	/** Rozamiento con el aire (1/s): con la velocidad de la carrera, empuja la lengua hacia atrás por la mejilla. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Face|Tongue", meta = (ClampMin = "0.0"))
	float TongueAirDrag = 2.f;

	/** Fuerza del aleteo al viento (multiplica la onda que recorre la lengua de la raíz a la punta; 1 = unos 10 cm de
	 *  arriba abajo en la punta a toda carrera). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Face|Tongue", meta = (ClampMin = "0.0"))
	float TongueFlap = 1.f;

	/** Quieta y contenta, de vez en cuando asoma la punta de la lengua. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Face|Tongue")
	bool bIdleBlep = true;

private:
	/** Puntos de la cadena de la lengua (la raíz incluida). */
	static constexpr int32 ChainNum = 8;

	/** Crea el ancla en la cabeza, la lengua y las gotas de sudor (false si la malla no es la de demo). */
	bool EnsureParts(USkeletalMeshComponent* Body);
	void DestroyParts();
	void BindChat();

	UFUNCTION()
	void HandleQuickChat(const FTN_QuickChatEntry& Entry);

	void UpdateMood(const ATortugaCharacter& Turtle);
	void UpdateTalk(float Dt);
	void BuildGoal(const ATortugaCharacter& Turtle, float Dt, FTNTurtleFaceGoal& Goal);
	void ApplyFaceMaterial(USkeletalMeshComponent* Body, float Dt, const FTNTurtleFaceGoal& Goal);
	void UpdateTongue(float Dt, const FTNTurtleFaceGoal& Goal, bool bRender);
	void SimulateTongue(float Dt, float LengthUnits, const FTNTurtleFaceGoal& Goal);
	void ResetTongueChain(const FTransform& FaceToWorld, const FVector& RootFace, const FVector& StartFace, const FVector& EndFace, float SegmentWorld);
	void RebuildTongueMesh(float LengthUnits, bool bCreate);
	void UpdateSweat(float Dt, const FTNTurtleFaceGoal& Goal, bool bRender);
	void SyncPartVisibility(USkeletalMeshComponent* Body);

	/** Ancla en el hueso Head con el marco de la malla en su postura de referencia (unidades y ejes de la malla). */
	UPROPERTY(Transient)
	TObjectPtr<USceneComponent> FaceRoot;

	UPROPERTY(Transient)
	TObjectPtr<UProceduralMeshComponent> TongueMesh;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UProceduralMeshComponent>> SweatDrops;

	TWeakObjectPtr<ATN_CoopGameState> ChatSource;
	TWeakObjectPtr<UProximityVoiceComponent> Voice;
	TWeakObjectPtr<UMaterialInstanceDynamic> FaceMID;

	bool bPartsTried = false;
	bool bPartsReady = false;
	/** M_TurtleFaceParts existe (el alfa del color de vértice es lo mojado); si no, se usa M_CosmeticVertexColor mate. */
	bool bWetAlpha = false;

	// ── Ánimo y cara ─────────────────────────────────────────────────────────
	ETNTurtleFaceMood Mood = ETNTurtleFaceMood::Happy;
	float FaceClock = 0.f;
	bool bWasExhausted = false;
	float SqueezeLeft = 0.f;
	float GaspTimer = 4.f;
	float BreathPhase = 0.f;
	float TiredShown = 0.f;
	float BlushShown = 0.f;
	float MouthShown = 0.3f;
	float SmileShown = 1.f;
	/** Últimos valores escritos en el material (-1 = hay que escribirlos, p. ej. tras cambiar de aspecto). */
	float TiredApplied = -1.f;
	float SqueezeApplied = -1.f;
	float MouthApplied = -1.f;
	float SmileApplied = -1.f;
	float BlushApplied = -1.f;

	// ── Hablar ───────────────────────────────────────────────────────────────
	float TalkLeft = 0.f;
	float SyllableTime = 0.f;
	float SyllableLength = 0.12f;
	float SyllablePeak = 0.f;
	float TalkLevel = 0.f;
	float VoiceSearchTimer = 0.f;

	// ── Lengua ───────────────────────────────────────────────────────────────
	float TongueOut = 0.f;
	float TongueLen = 0.f;
	float TongueSide = 0.f;
	float TongueStiffShown = 1.f;
	float SideGoal = 1.f;
	bool bWasSideTongue = false;
	float DownSide = 1.f;
	float PrevYaw = 0.f;
	float YawRate = 0.f;
	float FlapPhase = 0.f;
	float BlepTimer = 9.f;
	float BlepLeft = 0.f;
	bool bChainValid = false;
	FVector ChainPos[ChainNum];
	FVector ChainVel[ChainNum];
	FVector LastRootWorld = FVector::ZeroVector;
	TArray<FVector> TongueVerts;
	TArray<FVector> TongueNormals;

	// ── Sudor ────────────────────────────────────────────────────────────────
	float SweatClock = 0.f;
	float SweatShown = 0.f;
};
