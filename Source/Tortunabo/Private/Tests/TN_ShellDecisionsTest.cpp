// Fase 1 de la mecánica de caparazón (issue #6): lógica pura de entrada y salida.
// Sin mundo, sin componentes — se testean las funciones de TN_ShellDecisions.h que
// UTN_ShellComponent usa en producción. Correr desde Session Frontend (categoría
// "Tortunabo.Shell") o headless:
//   UnrealEditor-Cmd <uproject> -ExecCmds="Automation RunTests Tortunabo.Shell; Quit" -nullrhi -unattended

#include "Misc/AutomationTest.h"
#include "Player/TN_ShellDecisions.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace TNShellTestHelpers
{
	/** Contexto de un personaje sano, fuera del agua y con las manos libres: puede entrar. */
	static TNShellLogic::FShellEnterContext MakeValidContext()
	{
		TNShellLogic::FShellEnterContext Context;
		Context.bIsSwimming = false;
		Context.bIsDead = false;
		Context.bIsKnockedDown = false;
		Context.bIsDiving = false;
		Context.bHasEquippedItem = false;
		return Context;
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// Entrar al caparazón
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTNShellEnterTest,
	"Tortunabo.Shell.Enter",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FTNShellEnterTest::RunTest(const FString& Parameters)
{
	using namespace TNShellLogic;
	using namespace TNShellTestHelpers;

	TestTrue(TEXT("Sano, fuera del agua y con las manos libres (de pie o en pleno salto) → entra"),
		CanEnterShell(MakeValidContext()));

	{
		FShellEnterContext Context = MakeValidContext();
		Context.bIsSwimming = true;
		TestFalse(TEXT("Nadando no se entra"), CanEnterShell(Context));
	}

	{
		FShellEnterContext Context = MakeValidContext();
		Context.bIsDead = true;
		TestFalse(TEXT("Muerto no se entra"), CanEnterShell(Context));
	}

	{
		FShellEnterContext Context = MakeValidContext();
		Context.bIsKnockedDown = true;
		TestFalse(TEXT("Derribado no se entra"), CanEnterShell(Context));
	}

	{
		FShellEnterContext Context = MakeValidContext();
		Context.bIsDiving = true;
		TestFalse(TEXT("Buceando no se entra"), CanEnterShell(Context));
	}

	{
		FShellEnterContext Context = MakeValidContext();
		Context.bHasEquippedItem = true;
		TestFalse(TEXT("Con un objeto en la mano no se entra"), CanEnterShell(Context));
	}

	{
		// Varias condiciones a la vez: el resultado sigue siendo negativo.
		FShellEnterContext Context = MakeValidContext();
		Context.bIsSwimming = true;
		Context.bIsDead = true;
		Context.bHasEquippedItem = true;
		TestFalse(TEXT("Varios impedimentos a la vez → no entra"), CanEnterShell(Context));
	}

	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Salir del caparazón — permanencia mínima
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTNShellExitTest,
	"Tortunabo.Shell.Exit",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FTNShellExitTest::RunTest(const FString& Parameters)
{
	using namespace TNShellLogic;

	constexpr float MinTime = 0.3f;

	TestFalse(TEXT("Salir en el mismo instante de entrar → bloqueado"),
		CanExitShell(0.f, MinTime));

	TestFalse(TEXT("Salir antes del mínimo → bloqueado"),
		CanExitShell(0.29f, MinTime));

	TestTrue(TEXT("Salir justo en el mínimo → permitido"),
		CanExitShell(MinTime, MinTime));

	TestTrue(TEXT("Salir pasado el mínimo → permitido"),
		CanExitShell(5.f, MinTime));

	TestTrue(TEXT("Sin permanencia mínima configurada se sale siempre"),
		CanExitShell(0.f, 0.f));

	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Instrumento de la bola: torbellino, hundida y saltos de velocidad
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTNShellMotionAnomalyTest,
	"Tortunabo.Shell.MotionAnomaly",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FTNShellMotionAnomalyTest::RunTest(const FString& Parameters)
{
	using namespace TNShellLogic;

	// Rodando normal por la arena (≈ 300 cm/s → ~12 rad/s), encima del terreno y con la velocidad suave: nada.
	{
		FShellMotionSample Sample;
		Sample.DeltaSeconds = 1.f / 60.f;
		Sample.AngularSpeed = 12.f;
		Sample.VelocityChange = 40.f;
		Sample.BottomDepthUnderTerrain = -2.f;
		Sample.AgeSeconds = 2.f;
		float Spin = 0.f;
		for (int32 Step = 0; Step < 120; ++Step)
		{
			TestTrue(TEXT("Rodar deprisa no es un torbellino"), ClassifyShellMotion(Sample, Spin) == EShellMotionAnomaly::None);
		}
		TestEqual(TEXT("Por debajo del umbral el acumulador de giro no crece"), Spin, 0.f);
	}

	// Girando al tope (15,7 rad/s): un rebote corto no cuenta; sostenido 0,4 s, sí.
	{
		FShellMotionSample Sample;
		Sample.DeltaSeconds = 0.1f;
		Sample.AngularSpeed = 15.7f;
		Sample.AgeSeconds = 2.f;
		float Spin = 0.f;
		TestTrue(TEXT("0,1 s al tope: todavía nada"), ClassifyShellMotion(Sample, Spin) == EShellMotionAnomaly::None);
		TestTrue(TEXT("0,2 s al tope: todavía nada"), ClassifyShellMotion(Sample, Spin) == EShellMotionAnomaly::None);
		TestTrue(TEXT("0,3 s al tope: todavía nada"), ClassifyShellMotion(Sample, Spin) == EShellMotionAnomaly::None);
		TestTrue(TEXT("0,4 s al tope: torbellino"), ClassifyShellMotion(Sample, Spin) == EShellMotionAnomaly::Spin);

		// Baja del umbral un paso: el acumulador vuelve a cero y hay que sostenerlo otra vez.
		Sample.AngularSpeed = 5.f;
		TestTrue(TEXT("Frena: nada"), ClassifyShellMotion(Sample, Spin) == EShellMotionAnomaly::None);
		TestEqual(TEXT("Frena: el acumulador vuelve a cero"), Spin, 0.f);
		Sample.AngularSpeed = 15.7f;
		TestTrue(TEXT("Vuelve al tope un paso: nada"), ClassifyShellMotion(Sample, Spin) == EShellMotionAnomaly::None);
	}

	// Bajo la arena: manda sobre todo lo demás; tocar la arena (unos cm) no cuenta.
	{
		FShellMotionSample Sample;
		Sample.DeltaSeconds = 1.f / 60.f;
		Sample.AgeSeconds = 2.f;
		float Spin = 0.f;
		Sample.BottomDepthUnderTerrain = 5.f;
		TestTrue(TEXT("5 cm dentro de la arena: nada"), ClassifyShellMotion(Sample, Spin) == EShellMotionAnomaly::None);
		Sample.BottomDepthUnderTerrain = 40.f;
		Sample.VelocityChange = 2000.f;
		TestTrue(TEXT("40 cm bajo el terreno: hundida (antes que el salto de velocidad)"), ClassifyShellMotion(Sample, Spin) == EShellMotionAnomaly::Sunk);
	}

	// Salto de velocidad: el del propio lanzamiento (recién nacida o recién relanzada) no cuenta; más tarde, sí.
	{
		FShellMotionSample Sample;
		Sample.DeltaSeconds = 1.f / 60.f;
		Sample.VelocityChange = 1500.f;
		float Spin = 0.f;
		Sample.AgeSeconds = 0.05f;
		TestTrue(TEXT("Salto en el lanzamiento: nada"), ClassifyShellMotion(Sample, Spin) == EShellMotionAnomaly::None);
		Sample.AgeSeconds = 1.f;
		TestTrue(TEXT("Salto sin lanzamiento: salto de velocidad"), ClassifyShellMotion(Sample, Spin) == EShellMotionAnomaly::VelocityJump);
		Sample.VelocityChange = 500.f;
		TestTrue(TEXT("Un choque normal (5 m/s de cambio): nada"), ClassifyShellMotion(Sample, Spin) == EShellMotionAnomaly::None);
	}

	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Bola bajo el terreno: recolocación en el servidor
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTNShellSunkRescueTest,
	"Tortunabo.Shell.SunkRescue",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FTNShellSunkRescueTest::RunTest(const FString& Parameters)
{
	using namespace TNShellLogic;

	// Las cinco bolas del registro del 29-09 estaban 1,7-2,1 m bajo la arena: se recolocan a la segunda muestra.
	{
		int32 Strikes = 0;
		TestFalse(TEXT("Primera muestra a 170 cm: todavía no (un paso de la física no cuenta)"), ShouldRescueSunkenBody(170.f, Strikes));
		TestTrue(TEXT("Segunda muestra seguida a 180 cm: se recoloca"), ShouldRescueSunkenBody(180.f, Strikes));
		TestEqual(TEXT("Tras recolocarla el contador vuelve a cero"), Strikes, 0);
	}
	{
		int32 Strikes = 0;
		TestFalse(TEXT("Rozando la arena (20 cm) no cuenta"), ShouldRescueSunkenBody(20.f, Strikes));
		TestFalse(TEXT("Una muestra hundida..."), ShouldRescueSunkenBody(40.f, Strikes));
		TestFalse(TEXT("...y sale sola: se olvida"), ShouldRescueSunkenBody(-5.f, Strikes));
		TestFalse(TEXT("Otra hundida suelta: todavía no"), ShouldRescueSunkenBody(40.f, Strikes));
	}

	TestTrue(TEXT("Subir 2 m es recolocarla en el sitio"), IsSunkLiftAllowed(200.f));
	TestFalse(TEXT("Subir 6 m es un teletransporte: a la red de seguridad"), IsSunkLiftAllowed(600.f));
	TestFalse(TEXT("Bajar nunca"), IsSunkLiftAllowed(-10.f));

	{
		FVector Linear(300.0, 0.0, -900.0);
		FVector Angular(0.0, 0.0, 15.7);
		SettleRescuedVelocity(Linear, Angular);
		TestEqual(TEXT("Sin velocidad hacia abajo"), Linear.Z, 0.0);
		TestEqual(TEXT("Conserva la horizontal"), Linear.X, 300.0);
		TestTrue(TEXT("Giro limitado a 6 rad/s"), Angular.Size() <= 6.0 + KINDA_SMALL_NUMBER);
	}
	{
		FVector Linear(0.0, 0.0, 250.0);
		FVector Angular(1.0, 0.0, 0.0);
		SettleRescuedVelocity(Linear, Angular);
		TestEqual(TEXT("Si subía, sigue subiendo"), Linear.Z, 250.0);
		TestEqual(TEXT("Un giro suave no se toca"), Angular.X, 1.0);
	}
	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Bloques sólidos de los enemigos: empuje propio sobre la bola
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTNShellBlockPushTest,
	"Tortunabo.Shell.EnemyBlockPush",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FTNShellBlockPushTest::RunTest(const FString& Parameters)
{
	using namespace TNShellLogic;
	const FVector Extent(150.0, 100.0, 60.0);
	const float Radius = 25.f;
	const float Speed = 350.f;

	{
		FVector Velocity(0.0, 0.0, -400.0);
		TestFalse(TEXT("Lejos del bloque: nada"), PushBallOutOfBlock(FVector(400.0, 0.0, 0.0), Extent, Radius, Speed, Velocity));
		TestEqual(TEXT("Lejos del bloque: no toca su velocidad"), Velocity.Z, -400.0);
	}
	{
		// El caso del torbellino: el bloque baja sobre la bola, que queda debajo de su borde y la aplasta contra la arena.
		FVector Velocity(0.0, 0.0, -600.0);
		TestTrue(TEXT("Bajo el borde del bloque: la saca"), PushBallOutOfBlock(FVector(160.0, 20.0, -70.0), Extent, Radius, Speed, Velocity));
		TestTrue(TEXT("Nunca hacia abajo"), Velocity.Z >= 0.0);
		TestTrue(TEXT("Sale por el lado más cercano (+X) a la velocidad mínima"), Velocity.X >= Speed - KINDA_SMALL_NUMBER);
		TestEqual(TEXT("Sin empujón de lado"), Velocity.Y, 0.0);
	}
	{
		FVector Velocity(0.0, 0.0, 0.0);
		TestTrue(TEXT("Dentro, junto a la cara -Y"), PushBallOutOfBlock(FVector(10.0, -110.0, 0.0), Extent, Radius, Speed, Velocity));
		TestTrue(TEXT("Sale por -Y"), Velocity.Y <= -Speed + KINDA_SMALL_NUMBER);
	}
	{
		FVector Velocity(800.0, 0.0, 0.0);
		TestTrue(TEXT("Ya sale deprisa por +X"), PushBallOutOfBlock(FVector(160.0, 0.0, 0.0), Extent, Radius, Speed, Velocity));
		TestEqual(TEXT("No la frena"), Velocity.X, 800.0);
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
