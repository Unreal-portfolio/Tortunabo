#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TN_TurtleDustComponent.generated.h"

namespace TNTurtleDust
{
	// Emisores, caché de superficie y memoria del fotograma anterior (Private/Player/TN_TurtleDustComponent.cpp).
	struct FState;
}

/**
 * Polvo del arrastre del panzazo, del color y el material del suelo: nube clara y granos de arena en la playa y el
 * desierto, polvo marrón y terrones en la tierra, polvo gris y arenilla en la roca, polvo claro y astillas en la madera
 * y salpicaduras en el agua poco profunda. En el terreno del mapa procedural, el polvo se tiñe con el camino del bioma.
 *
 * - Al caer de tripa, una bocanada alrededor (más grande cuanto más fuerte cae) y, en el agua, un chapuzón.
 * - Mientras se arrastra, polvo que sale de delante y de los lados hacia atrás y arriba, tanto más cuanto más deprisa.
 * - Al chocar arrastrándose, una bocanada pequeña contra el obstáculo.
 *
 * Cosmético y local en cada máquina, sin RPC: lee el estado replicado de la tortuga (ATortugaCharacter::IsBellyOnGround,
 * la velocidad). Las partículas son instancias de mallas de caras planas (TNAmbientFX, las mismas de los géiseres y el
 * rebuscar), movidas aquí; los emisores se crean la primera vez que hacen falta y solo se mueven con partículas vivas.
 * Nada en servidor dedicado; lejos de la cámara local no se levanta polvo.
 */
UCLASS(ClassGroup = (Effects), meta = (BlueprintSpawnableComponent))
class TORTUNABO_API UTN_TurtleDustComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UTN_TurtleDustComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/**
	 * El componente de InOwner; si no tiene, se lo crea (registrado). Null en servidor dedicado, fuera de un mundo de
	 * juego o con el actor destruyéndose.
	 */
	static UTN_TurtleDustComponent* FindOrAddTo(AActor* InOwner);

	/** Cantidad de polvo (1 = normal; multiplica lo que nace). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TurtleDust", meta = (ClampMin = "0.0", ClampMax = "4.0"))
	float DustAmount = 1.f;

	/** Más lejos de la cámara local (cm) no se levanta polvo (lo que ya vuela acaba su vida). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TurtleDust", meta = (ClampMin = "500.0"))
	float MaxViewDistance = 5000.f;

	/** Velocidad del arrastre (cm/s) con el polvo a tope. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TurtleDust", meta = (ClampMin = "100.0"))
	float FullDustSpeed = 650.f;

private:
	/** Emisores (uno de polvo y otro de trocitos por superficie y color), caché de superficie y fotograma anterior. */
	TSharedPtr<TNTurtleDust::FState> State;
};
