// Manos VR (VR/TN_VRHandMath.h, VR/TN_VRGrabComponent.h, ATN_VRRig::BlockHandLocation; Docs/Modo_VR.md): velocidad de la
// mano para lanzar, agarres enganchados que se sueltan, viñeta de confort, sitio para el HUD, manos que no atraviesan el
// escenario y objetos que no se quitan de la mano a otro jugador. Se pueden correr sin gafas.
//   UnrealEditor-Cmd <uproject> -ExecCmds="Automation RunTests Tortunabo.VR; Quit" -nullrhi -unattended

#include "Misc/AutomationTest.h"
#include "CollisionQueryParams.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "VR/TN_VRGrabComponent.h"
#include "VR/TN_VRHandMath.h"
#include "VR/TN_VRMath.h"
#include "VR/TN_VRRig.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace TNVRHandsTest
{
	/** Velocidad mínima de la mano para que soltar sea lanzar (ATortugaCharacter::VRThrowSpeed de serie). */
	constexpr float ThrowSpeed = 250.f;

	/** Mundo de juego sin ventana para las pruebas con colisión (se destruye al salir del ámbito). */
	struct FTestWorld
	{
		UWorld* World = nullptr;

		FTestWorld()
		{
			World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("TNVRHandsTestWorld"));
			if (World && GEngine)
			{
				FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
				Context.SetCurrentWorld(World);
				World->InitializeActorsForPlay(FURL());
			}
		}

		~FTestWorld()
		{
			if (World && GEngine)
			{
				GEngine->DestroyWorldContext(World);
				World->DestroyWorld(false);
			}
		}

		/** Una caja (el cubo de 100 cm del motor) en Location con Scale; con física si bPhysics. */
		AStaticMeshActor* SpawnBox(const FVector& Location, const FVector& Scale, bool bPhysics) const
		{
			UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
			AStaticMeshActor* Box = World ? World->SpawnActor<AStaticMeshActor>(Location, FRotator::ZeroRotator) : nullptr;
			if (!Box || !Cube)
			{
				return nullptr;
			}
			Box->SetReplicates(false);
			UStaticMeshComponent* Mesh = Box->GetStaticMeshComponent();
			Mesh->SetMobility(EComponentMobility::Movable);
			Mesh->SetStaticMesh(Cube);
			Mesh->SetWorldScale3D(Scale);
			Mesh->SetCollisionProfileName(bPhysics ? TEXT("PhysicsActor") : TEXT("BlockAll"));
			Mesh->SetSimulatePhysics(bPhysics);
			return Box;
		}

		/** Un actor con UTN_VRGrabComponent (la tortuga de la prueba), en Location. */
		UTN_VRGrabComponent* SpawnGrabber(const FVector& Location) const
		{
			AActor* Owner = World ? World->SpawnActor<AActor>(Location, FRotator::ZeroRotator) : nullptr;
			if (!Owner)
			{
				return nullptr;
			}
			USceneComponent* Root = NewObject<USceneComponent>(Owner, TEXT("Root"));
			Owner->SetRootComponent(Root);
			Root->RegisterComponent();
			Root->SetWorldLocation(Location);
			UTN_VRGrabComponent* Grab = NewObject<UTN_VRGrabComponent>(Owner, TEXT("VRGrab"));
			Grab->RegisterComponent();
			return Grab;
		}
	};
}

// ─────────────────────────────────────────────────────────────────────────────
// Velocidad de la mano: media de los últimos 70 ms
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTNVRHandVelocityWindowTest,
	"Tortunabo.VR.HandVelocityWindow",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FTNVRHandVelocityWindowTest::RunTest(const FString& Parameters)
{
	using TNVRHands::FHandVelocityWindow;
	const FVector Swing(400.0, 0.0, 0.0);

	// El mismo gesto a 72 y a 120 Hz: la misma velocidad.
	FHandVelocityWindow At72;
	FHandVelocityWindow At120;
	for (int32 i = 0; i < 20; ++i)
	{
		At72.Add(Swing, 1.f / 72.f);
		At120.Add(Swing, 1.f / 120.f);
	}
	TestTrue(TEXT("Gesto a 72 Hz: 400 cm/s"), At72.Average().Equals(Swing, 0.01));
	TestTrue(TEXT("Gesto a 120 Hz: 400 cm/s"), At120.Average().Equals(Swing, 0.01));
	TestTrue(TEXT("Gesto de 400 cm/s: lanza"), TNVRMath::IsThrowSwing(At72.Average(), TNVRHandsTest::ThrowSpeed));

	// Mano quieta con un tirón del seguimiento de un solo fotograma (90 Hz, 600 cm/s): no lanza. Con el suavizado de antes
	// (la mitad de lo nuevo en cada fotograma) habría salido a 300 cm/s, por encima del umbral.
	FHandVelocityWindow Jitter;
	for (int32 i = 0; i < 10; ++i)
	{
		Jitter.Add(FVector::ZeroVector, 1.f / 90.f);
	}
	const FVector Spike(600.0, 0.0, 0.0);
	Jitter.Add(Spike, 1.f / 90.f);
	const FVector OldSmoothing = FMath::Lerp(FVector::ZeroVector, Spike, 0.5f);
	TestTrue(TEXT("Antes: el tirón suelto lanzaba"), TNVRMath::IsThrowSwing(OldSmoothing, TNVRHandsTest::ThrowSpeed));
	TestFalse(TEXT("Ahora: el tirón suelto no lanza"), TNVRMath::IsThrowSwing(Jitter.Average(), TNVRHandsTest::ThrowSpeed));

	// Lo de hace más de 70 ms no cuenta: tras un gesto, 10 fotogramas quietos a 90 Hz (111 ms) dejan la mano en 0.
	FHandVelocityWindow Stopped;
	for (int32 i = 0; i < 10; ++i)
	{
		Stopped.Add(Swing, 1.f / 90.f);
	}
	for (int32 i = 0; i < 10; ++i)
	{
		Stopped.Add(FVector::ZeroVector, 1.f / 90.f);
	}
	TestTrue(TEXT("Lo de hace más de 70 ms se olvida"), Stopped.Average().IsNearlyZero(0.01));

	// Un fotograma largo (un tirón de rendimiento de 100 ms) cuenta solo lo que cabe en la ventana: su velocidad.
	FHandVelocityWindow Hitch;
	Hitch.Add(FVector::ZeroVector, 1.f / 90.f);
	Hitch.Add(Swing, 0.1f);
	TestTrue(TEXT("Fotograma largo: su velocidad"), Hitch.Average().Equals(Swing, 0.01));

	// Sin tiempo no se añade nada; al empezar de cero, 0.
	FHandVelocityWindow Empty;
	Empty.Add(Swing, 0.f);
	TestEqual(TEXT("Sin tiempo: no añade"), Empty.Num(), 0);
	TestTrue(TEXT("Vacía: 0"), Empty.Average().IsZero());
	At72.Reset();
	TestTrue(TEXT("Tras Reset: 0"), At72.Average().IsZero());
	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Agarre enganchado: se suelta solo
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTNVRGrabStrainTest,
	"Tortunabo.VR.GrabStrain",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FTNVRGrabStrainTest::RunTest(const FString& Parameters)
{
	using namespace TNVRHands;
	double Since = -1.0;
	TestFalse(TEXT("En la mano: no se suelta"), ShouldBreakGrab(10.f, 0.0, Since));
	TestTrue(TEXT("En la mano: no va enganchado"), Since < 0.0);

	// Detrás de una pared (60 cm de la mano): aguanta un momento y se suelta pasado GrabStrainSeconds.
	TestFalse(TEXT("Enganchado: el primer momento aguanta"), ShouldBreakGrab(60.f, 1.0, Since));
	TestTrue(TEXT("Enganchado: desde 1 s"), FMath::IsNearlyEqual(Since, 1.0));
	TestFalse(TEXT("Enganchado 0,2 s: aguanta (un gesto rápido también separa)"), ShouldBreakGrab(60.f, 1.2, Since));
	TestTrue(TEXT("Enganchado 0,4 s: se suelta"), ShouldBreakGrab(60.f, 1.4, Since));

	// Si vuelve a la mano antes, se empieza de cero.
	double Back = -1.0;
	ShouldBreakGrab(60.f, 2.0, Back);
	TestFalse(TEXT("Vuelve a la mano: no se suelta"), ShouldBreakGrab(20.f, 2.2, Back));
	TestFalse(TEXT("Otra vez lejos: empieza de cero"), ShouldBreakGrab(60.f, 2.3, Back));
	TestFalse(TEXT("Otra vez lejos 0,2 s: aguanta"), ShouldBreakGrab(60.f, 2.5, Back));

	// Muy lejos (la tortuga se ha teletransportado): al momento.
	double Snap = -1.0;
	TestTrue(TEXT("A 2 m: se suelta al momento"), ShouldBreakGrab(200.f, 0.0, Snap));

	// La vibración del tirón: nada cerca de la mano, sube al acercarse a soltarse y no pasa de 0,5.
	TestEqual(TEXT("Tirón: 0 en la mano"), StrainAmplitude(10.f), 0.f);
	TestTrue(TEXT("Tirón: sube con la separación"), StrainAmplitude(40.f) > StrainAmplitude(30.f));
	TestTrue(TEXT("Tirón: como mucho 0,5"), FMath::IsNearlyEqual(StrainAmplitude(500.f), 0.5f));
	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Pulsar botones con la punta de la aleta
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTNVRPokeTest,
	"Tortunabo.VR.Poke",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FTNVRPokeTest::RunTest(const FString& Parameters)
{
	using namespace TNVRHands;
	constexpr float Dt = 1.f / 90.f;
	// Acercar la punta al botón a 90 cm/s (1 cm por fotograma): pulsa una vez al tocarlo.
	FPokeState Press;
	int32 Presses = 0;
	double Now = 0.0;
	for (float Distance = 9.f; Distance >= 0.f; Distance -= 1.f)
	{
		Presses += UpdatePoke(Press, Distance, Dt, Now) ? 1 : 0;
		Now += Dt;
	}
	TestEqual(TEXT("Acercar la punta: una pulsación"), Presses, 1);
	// La mano se queda apoyada (o metida en el botón) un segundo: no vuelve a pulsar.
	for (int32 i = 0; i < 90; ++i)
	{
		Presses += UpdatePoke(Press, 0.f, Dt, Now) ? 1 : 0;
		Now += Dt;
	}
	TestEqual(TEXT("Mano apoyada: no repite"), Presses, 1);
	// Se aleja más de PokeRearmDistance y vuelve: otra pulsación.
	UpdatePoke(Press, PokeRearmDistance + 5.f, Dt, Now);
	Now += Dt;
	for (float Distance = 9.f; Distance >= 0.f; Distance -= 1.f)
	{
		Presses += UpdatePoke(Press, Distance, Dt, Now) ? 1 : 0;
		Now += Dt;
	}
	TestEqual(TEXT("Alejarse y volver: segunda pulsación"), Presses, 2);

	// Apoyar la mano despacio (10 cm/s): no pulsa.
	FPokeState Slow;
	bool bSlowPress = false;
	for (float Distance = 5.f; Distance >= 0.f; Distance -= 0.1f)
	{
		bSlowPress |= UpdatePoke(Slow, Distance, 0.01f, 0.0);
	}
	TestFalse(TEXT("Apoyar despacio: no pulsa"), bSlowPress);

	// La mano aparece ya tocándolo (la para el escenario encima, se recentra): no pulsa.
	FPokeState Appear;
	TestFalse(TEXT("Aparece tocándolo: no pulsa"), UpdatePoke(Appear, 1.f, Dt, 0.0));
	TestFalse(TEXT("Sin botón cerca: no pulsa"), UpdatePoke(Appear, -1.f, Dt, 0.0));
	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Gatillo entre los menús y el juego
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTNVRTriggerMenuLatchTest,
	"Tortunabo.VR.TriggerMenuLatch",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FTNVRTriggerMenuLatchTest::RunTest(const FString& Parameters)
{
	using TNVRHands::ShouldEatTriggerAxis;
	// Clic en «Cerrar» de la tienda con el gatillo: se aprieta con el menú delante, el menú se cierra y el gatillo sigue
	// apretado unos fotogramas (con su temblor). Antes el juego lo veía como una pulsación nueva y volvía a abrir la tienda.
	bool bPressed = false;
	TestTrue(TEXT("Apretado en el menú: es del menú"), ShouldEatTriggerAxis(true, true, 0.9f, bPressed));
	TestTrue(TEXT("Menú cerrado, aún apretado: no llega al juego"), ShouldEatTriggerAxis(false, false, 0.88f, bPressed));
	TestTrue(TEXT("Menú cerrado, aún apretado (otro valor): tampoco"), ShouldEatTriggerAxis(false, false, 0.6f, bPressed));
	TestFalse(TEXT("Al soltarlo: llega al juego (lo ve abierto)"), ShouldEatTriggerAxis(false, false, 0.1f, bPressed));
	TestFalse(TEXT("Apretado otra vez jugando: llega al juego"), ShouldEatTriggerAxis(false, false, 0.9f, bPressed));

	// El menú se abrió con el gatillo (interactuar con la tienda) y se suelta dentro: el juego tiene que ver que se soltó, o
	// se queda con el 90 % de antes del menú.
	bool bOpened = false;
	TestFalse(TEXT("Jugando: llega al juego"), ShouldEatTriggerAxis(false, true, 0.9f, bOpened));
	TestTrue(TEXT("Menú abierto, aún apretado: es del menú"), ShouldEatTriggerAxis(true, false, 0.9f, bOpened));
	TestFalse(TEXT("Soltado con el menú delante: llega al juego"), ShouldEatTriggerAxis(true, false, 0.f, bOpened));
	TestFalse(TEXT("Al cerrarlo sin tocar: nada pendiente"), bOpened);
	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Viñeta de confort y sitio para el HUD
// ─────────────────────────────────────────────────────────────────────────────



IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTNVRComfortVignetteTest,
	"Tortunabo.VR.ComfortVignette",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FTNVRComfortVignetteTest::RunTest(const FString& Parameters)
{
	using namespace TNVRHands;
	TestEqual(TEXT("Parada: sin viñeta"), ComfortVignette(0.f, 0.f, 1.f), 0.f);
	TestEqual(TEXT("Andando despacio (por debajo del umbral): sin viñeta"), ComfortVignette(VignetteStartSpeed, 0.f, 1.f), 0.f);
	TestTrue(TEXT("Andando: algo"), ComfortVignette(500.f, 0.f, 1.f) > 0.f);
	TestTrue(TEXT("Más deprisa, más oscuro"), ComfortVignette(800.f, 0.f, 1.f) > ComfortVignette(500.f, 0.f, 1.f));
	TestTrue(TEXT("Lanzada (catapulta): a tope"), FMath::IsNearlyEqual(ComfortVignette(3000.f, 0.f, 1.f), VignetteMaxIntensity));
	TestTrue(TEXT("Giro suave parada: a tope"), FMath::IsNearlyEqual(ComfortVignette(0.f, -120.f, 1.f), VignetteMaxIntensity));
	TestEqual(TEXT("Apagada (0): nada"), ComfortVignette(3000.f, 120.f, 0.f), 0.f);
	TestTrue(TEXT("Fuerza 2: el doble"), FMath::IsNearlyEqual(ComfortVignette(3000.f, 0.f, 2.f), 2.f * VignetteMaxIntensity));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTNVRHudProbeTest,
	"Tortunabo.VR.HudProbe",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FTNVRHudProbeTest::RunTest(const FString& Parameters)
{
	using namespace TNVRHands;
	constexpr float Arc = 80.f;
	constexpr float Aspect = 1080.f / 1920.f;
	TestTrue(TEXT("Centro: de frente"), HudProbeDirection(Arc, Aspect, 0.5f, 0.5f).Equals(FVector::ForwardVector, 1e-4));
	const FVector Left = HudProbeDirection(Arc, Aspect, 0.f, 0.5f);
	TestTrue(TEXT("Borde izquierdo: 40° a la izquierda"), FMath::IsNearlyEqual(FMath::RadiansToDegrees(FMath::Atan2(Left.Y, Left.X)), -40.0, 1e-3));
	const FVector Bottom = HudProbeDirection(Arc, Aspect, 0.5f, 1.f);
	TestTrue(TEXT("Borde de abajo: por debajo de los ojos"), Bottom.Z < 0.0);
	// Alto del panel = ancho del arco × 1080/1920: la mitad, 0,39 radios por debajo (21° con 80° de arco).
	TestTrue(TEXT("Borde de abajo: unos 21°"), FMath::IsNearlyEqual(FMath::RadiansToDegrees(FMath::Asin(-Bottom.Z)), 21.4, 0.2));

	// El suelo corta el borde de abajo a 120 cm de los ojos: el HUD tiene que acercarse a 120 × cos(21°) ≈ 112 cm (antes solo
	// se miraba el centro, que no toca el suelo, y el suelo tapaba la parte de abajo).
	TestTrue(TEXT("Suelo en el borde de abajo: radio de 112 cm"), FMath::IsNearlyEqual(HudRadiusForHit(Bottom, 120.f), 111.7f, 0.5f));
	TestTrue(TEXT("De frente: el radio es la distancia"), FMath::IsNearlyEqual(HudRadiusForHit(FVector::ForwardVector, 90.f), 90.f));
	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Con mundo: manos contra una pared y objetos que lleva otro
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTNVRHandBlockTest,
	"Tortunabo.VR.HandBlock",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FTNVRHandBlockTest::RunTest(const FString& Parameters)
{
	TNVRHandsTest::FTestWorld Test;
	// Pared de 10 cm de grueso con su cara de delante a 45 cm de los ojos (centro en X = 50).
	AStaticMeshActor* Wall = Test.SpawnBox(FVector(50.0, 0.0, 0.0), FVector(0.1, 3.0, 3.0), false);
	if (!TestNotNull(TEXT("Mundo y pared"), Wall))
	{
		return false;
	}
	const FCollisionQueryParams Params(SCENE_QUERY_STAT(TNVRHandBlockTest), false);
	constexpr float Radius = 4.f;
	FVector Out;

	// La mano al otro lado de la pared (el mando ha entrado en ella): se queda en su cara, con el radio de holgura.
	TestTrue(TEXT("Mano detrás de la pared: parada"), ATN_VRRig::BlockHandLocation(Test.World, FVector::ZeroVector, FVector(80.0, 0.0, 0.0), Radius, Params, Out));
	TestTrue(TEXT("Parada en la cara de la pared"), FMath::IsNearlyEqual(Out.X, 45.0 - Radius, 1.0));

	// Delante de la pared: llega.
	TestFalse(TEXT("Mano delante de la pared: llega"), ATN_VRRig::BlockHandLocation(Test.World, FVector::ZeroVector, FVector(30.0, 0.0, 0.0), Radius, Params, Out));
	TestTrue(TEXT("Delante: donde está el mando"), Out.Equals(FVector(30.0, 0.0, 0.0)));

	// Ojos dentro de la pared: no se sabe el otro lado; la mano va a su mando.
	TestFalse(TEXT("Ojos dentro de la pared: no se para"), ATN_VRRig::BlockHandLocation(Test.World, FVector(50.0, 0.0, 0.0), FVector(80.0, 0.0, 0.0), Radius, Params, Out));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTNVRGrabHolderTest,
	"Tortunabo.VR.GrabHolder",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FTNVRGrabHolderTest::RunTest(const FString& Parameters)
{
	TNVRHandsTest::FTestWorld Test;
	// Una caja con física de 20 cm y dos «tortugas» a su lado.
	AStaticMeshActor* Box = Test.SpawnBox(FVector(0.0, 0.0, 200.0), FVector(0.2), true);
	UTN_VRGrabComponent* First = Test.SpawnGrabber(FVector(-60.0, 0.0, 200.0));
	UTN_VRGrabComponent* Second = Test.SpawnGrabber(FVector(60.0, 0.0, 200.0));
	if (!TestNotNull(TEXT("Caja"), Box) || !TestNotNull(TEXT("Primera"), First) || !TestNotNull(TEXT("Segunda"), Second))
	{
		return false;
	}
	UPrimitiveComponent* Mesh = Box->GetStaticMeshComponent();
	TestTrue(TEXT("La caja simula física"), Mesh->IsSimulatingPhysics());
	const FTransform LeftOfBox(FVector(-12.0, 0.0, 200.0));
	const FTransform RightOfBox(FVector(12.0, 0.0, 200.0));

	TestTrue(TEXT("La primera la coge"), First->TryGrab(1, LeftOfBox));
	TestTrue(TEXT("La lleva la primera"), UTN_VRGrabComponent::FindHolder(Mesh) == First);
	// Antes la segunda también la cogía y las dos manos tiraban de ella.
	TestFalse(TEXT("La segunda no se la quita"), Second->TryGrab(1, RightOfBox));
	TestTrue(TEXT("La primera, con la otra mano, sí"), First->TryGrab(0, RightOfBox));

	First->Release(1, FVector::ZeroVector);
	TestTrue(TEXT("Con una mano aún, sigue siendo suya"), UTN_VRGrabComponent::FindHolder(Mesh) == First);
	First->Release(0, FVector::ZeroVector);
	TestNull(TEXT("Soltada con las dos: libre"), UTN_VRGrabComponent::FindHolder(Mesh));
	TestTrue(TEXT("Ahora la segunda la coge"), Second->TryGrab(1, RightOfBox));
	Second->Release(1, FVector::ZeroVector);
	return true;
}

#endif
