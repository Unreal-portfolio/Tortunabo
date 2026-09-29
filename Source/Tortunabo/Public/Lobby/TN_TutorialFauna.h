#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TN_TutorialFauna.generated.h"

class ATN_TutorialCourse;
class UInstancedStaticMeshComponent;
class USceneComponent;
class UStaticMesh;

/**
 * @brief Fauna ambiental del tutorial: los animales del mapa procedural (mallas por piezas de TN_ProcMapFaunaMeshes.h:
 * monos y tucanes en la selva, marmotas y cabras en la roca, cangrejos y gaviotas en la playa, suricatos y lagartijas en el
 * desierto, flamencos y garzas en la laguna, cangrejos violinistas en el manglar, gallinas y palomas en el pueblo), en sitios
 * fijos junto al pasillo de cada bioma.
 *
 * Una versión corta de la de ATN_ProcFauna (que depende del generador del mapa): cada animal está en reposo con su gesto
 * (mira, picotea, se acicala, pasta, agita las pinzas...), da paseítos alrededor de su sitio y, si se acerca una tortuga,
 * huye a su manera (echa a volar, se entierra o sale corriendo) y vuelve un rato después. Solo visual y local: sin
 * colisión, sin red, y solo se anima lo que está cerca de la cámara.
 */
UCLASS()
class TORTUNABO_API ATN_TutorialFauna : public AActor
{
	GENERATED_BODY()

public:
	ATN_TutorialFauna();

	virtual void Tick(float DeltaSeconds) override;

	/** Coloca los animales del recorrido (en el espacio del mundo del recorrido). */
	void Init(const ATN_TutorialCourse* InCourse);

	/** Muestra u oculta todos (el recorrido solo se ve con la cámara arriba). */
	void SetFaunaVisible(bool bInVisible);

private:
	UPROPERTY(VisibleAnywhere, Category = "Fauna")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UInstancedStaticMeshComponent>> PartISMs;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMesh>> PartMeshes;

	struct FAnimal
	{
		FVector Home = FVector::ZeroVector;
		FVector Pos = FVector::ZeroVector;
		FVector Goal = FVector::ZeroVector;
		FVector FleeDir = FVector::ForwardVector;
		float Yaw = 0.f;
		float Size = 1.f;
		float Presence = 1.f;
		float StateT = 0.f;
		float Dur = 2.f;
		float Gait = 0.f;
		float Look = 0.f;
		float LookGoal = 0.f;
		float Clock = 0.f;
		float Air = 0.f;
		float Open = 0.f;
		float FlapPh = 0.f;
		int32 Kind = 0;
		int32 Slot = 0;
		uint8 Species = 0;
		/** 0 reposo, 1 paseo, 2 huida, 3 escondido. */
		uint8 State = 0;
		uint8 Act = 0;
		bool bShown = false;
	};

	struct FKind
	{
		uint8 Species = 0;
		int32 FirstPart = 0;
		int32 NumParts = 0;
		float BodyZ = 0.f;
		float Height = 0.f;
		TArray<int32> Members;
		int32 DirtyMin = MAX_int32;
		int32 DirtyMax = -1;
	};

	TWeakObjectPtr<const ATN_TutorialCourse> Course;
	TArray<FAnimal> Animals;
	TArray<FKind> Kinds;
	TArray<uint8> PartBone;
	TArray<FVector> PartPivot;
	TArray<TArray<FTransform>> PartXf;
	TArray<FVector> Threats;
	FVector ViewLoc = FVector::ZeroVector;
	bool bVisible = true;
	uint32 Rng = 0x51A7E5u;

	float RandUnit();
	float GroundAt(const FVector& WorldPoint) const;
	void Simulate(FAnimal& A, float Dt);
	void Write(FAnimal& A);
	void WriteHidden(FAnimal& A);
	void PoseBones(const FAnimal& A, FTransform* OutBones) const;
};
