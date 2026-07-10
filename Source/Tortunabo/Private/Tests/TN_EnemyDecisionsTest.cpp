// Primeros tests del módulo (Fase 4.3): lógica pura de decisiones de enemigos.
// Sin mundo, sin actores — se testean las funciones de TN_EnemyDecisions.h que
// los actores usan en producción. Correr desde Session Frontend (categoría
// "Tortunabo.Enemies") o headless:
//   UnrealEditor-Cmd <uproject> -ExecCmds="Automation RunTests Tortunabo.Enemies; Quit" -nullrhi -unattended

#include "Misc/AutomationTest.h"
#include "World/TN_EnemyDecisions.h"

#if WITH_DEV_AUTOMATION_TESTS

// ─────────────────────────────────────────────────────────────────────────────
// Gaviota — countdown por timestamp
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTNSeagullCountdownTest,
	"Tortunabo.Enemies.Seagull.Countdown",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FTNSeagullCountdownTest::RunTest(const FString& Parameters)
{
	using namespace TNSeagullLogic;

	// Sin inicializar (timestamp aún no replicado) → countdown completo
	TestEqual(TEXT("Sin inicializar devuelve la duración completa"),
		ComputeCountdownRemaining(-1.f, 100.f, 8.f), 8.f);

	// Transcurso parcial: arrancó en t=100, ahora t=103 → quedan 5 de 8
	TestEqual(TEXT("Elapsed parcial descuenta correctamente"),
		ComputeCountdownRemaining(100.f, 103.f, 8.f), 5.f);

	// Expirado hace rato → clamp a 0, nunca negativo
	TestEqual(TEXT("Expirado clampa a 0"),
		ComputeCountdownRemaining(100.f, 200.f, 8.f), 0.f);

	// Reloj del cliente por detrás del timestamp (jitter de sincronización JIP):
	// elapsed negativo → clamp superior a la duración, nunca más
	TestEqual(TEXT("Elapsed negativo clampa a la duración total"),
		ComputeCountdownRemaining(100.f, 99.f, 8.f), 8.f);

	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Gaviota — radio de peligro
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTNSeagullDangerRadiusTest,
	"Tortunabo.Enemies.Seagull.DangerRadius",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FTNSeagullDangerRadiusTest::RunTest(const FString& Parameters)
{
	using namespace TNSeagullLogic;

	// Countdown completo → radio máximo
	TestEqual(TEXT("Countdown completo = radio máximo"),
		ComputeDangerRadius(8.f, 8.f, 150.f, 500.f), 500.f);

	// Countdown expirado → radio de kill mínimo
	TestEqual(TEXT("Countdown a 0 = radio mínimo"),
		ComputeDangerRadius(0.f, 8.f, 150.f, 500.f), 150.f);

	// Mitad del countdown → punto medio del lerp
	TestEqual(TEXT("Mitad del countdown = lerp al 50%"),
		ComputeDangerRadius(4.f, 8.f, 150.f, 500.f), 325.f);

	// Config degenerada (AttackTimerSeconds=0) → nunca divide por cero
	TestEqual(TEXT("Timer 0 devuelve el radio mínimo sin dividir por cero"),
		ComputeDangerRadius(0.f, 0.f, 150.f, 500.f), 150.f);

	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Gaviota — decisión de ataque (precedencia target > techo > distancia)
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTNSeagullAttackDecisionTest,
	"Tortunabo.Enemies.Seagull.AttackDecision",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FTNSeagullAttackDecisionTest::RunTest(const FString& Parameters)
{
	using namespace TNSeagullLogic;

	TestTrue(TEXT("Target perdido → retreat (aunque no haya techo y esté cerca)"),
		DecideAttack(false, false, 0.f, 150.f) == EAttackDecision::Retreat_TargetLost);

	TestTrue(TEXT("Techo entre gaviota y target → retreat"),
		DecideAttack(true, true, 0.f, 150.f) == EAttackDecision::Retreat_Roof);

	TestTrue(TEXT("Target fuera del radio de kill → escaped"),
		DecideAttack(true, false, 150.01f, 150.f) == EAttackDecision::Retreat_Escaped);

	// Borde de gameplay: EXACTAMENTE en el radio de kill NO es escape → muere.
	// (El escape exige estrictamente >; si esto cambia, cambia el feel del juego.)
	TestTrue(TEXT("Dist == MinKillRadius → strike (el borde mata)"),
		DecideAttack(true, false, 150.f, 150.f) == EAttackDecision::Strike);

	TestTrue(TEXT("Target válido, sin techo, dentro del radio → strike"),
		DecideAttack(true, false, 50.f, 150.f) == EAttackDecision::Strike);

	// Precedencia: target perdido gana al techo
	TestTrue(TEXT("Target perdido tiene precedencia sobre el techo"),
		DecideAttack(false, true, 999.f, 150.f) == EAttackDecision::Retreat_TargetLost);

	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Gaviota — temporizador de escape
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTNSeagullEscapeTimerTest,
	"Tortunabo.Enemies.Seagull.EscapeTimer",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FTNSeagullEscapeTimerTest::RunTest(const FString& Parameters)
{
	using namespace TNSeagullLogic;
	bool bAbort = false;

	// Fuera del radio: acumula sin abortar mientras no llegue a EscapeSeconds
	TestEqual(TEXT("Fuera del radio acumula DeltaTime"),
		AdvanceEscapeTimer(600.f, 500.f, 1.0f, 0.5f, 3.f, bAbort), 1.5f);
	TestFalse(TEXT("1.5s de 3s no aborta"), bAbort);

	// Cruza el umbral → abort
	AdvanceEscapeTimer(600.f, 500.f, 2.9f, 0.2f, 3.f, bAbort);
	TestTrue(TEXT("Acumulado >= EscapeSeconds aborta"), bAbort);

	// Borde exacto: llegar justo a EscapeSeconds también aborta (>=)
	AdvanceEscapeTimer(600.f, 500.f, 2.5f, 0.5f, 3.f, bAbort);
	TestTrue(TEXT("Acumulado == EscapeSeconds aborta (borde >=)"), bAbort);

	// Volver dentro del radio resetea el acumulado a 0 y no aborta
	TestEqual(TEXT("Dentro del radio resetea a 0"),
		AdvanceEscapeTimer(400.f, 500.f, 2.9f, 0.5f, 3.f, bAbort), 0.f);
	TestFalse(TEXT("Dentro del radio nunca aborta"), bAbort);

	// Borde: EXACTAMENTE en el radio cuenta como dentro (escape exige >)
	TestEqual(TEXT("Dist == radio cuenta como dentro"),
		AdvanceEscapeTimer(500.f, 500.f, 2.9f, 0.5f, 3.f, bAbort), 0.f);

	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Cangrejo — efectos (stun/blind) por timestamp
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTNCrabEffectTimestampTest,
	"Tortunabo.Enemies.Crab.EffectTimestamps",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FTNCrabEffectTimestampTest::RunTest(const FString& Parameters)
{
	using namespace TNCrabLogic;

	// Restante básico
	TestEqual(TEXT("Efecto activo devuelve el restante"),
		ComputeEffectRemaining(105.f, 100.f), 5.f);

	// Expirado → 0, nunca negativo
	TestEqual(TEXT("Efecto expirado devuelve 0"),
		ComputeEffectRemaining(100.f, 105.f), 0.f);

	// Sin efecto aplicado nunca (end=0) → 0
	TestEqual(TEXT("Sin efecto (end=0) devuelve 0"),
		ComputeEffectRemaining(0.f, 100.f), 0.f);

	// Regla de gameplay: un efecto nuevo CORTO no acorta uno en curso más largo.
	// Quedan 5s (end=105) y aplican 2s (end=102) → sigue expirando en 105.
	TestEqual(TEXT("Aplicar efecto corto no acorta uno largo en curso"),
		ExtendEffectEndTime(105.f, 100.f, 2.f), 105.f);

	// Un efecto más largo sí extiende
	TestEqual(TEXT("Aplicar efecto más largo extiende la expiración"),
		ExtendEffectEndTime(105.f, 100.f, 8.f), 108.f);

	// Primer efecto sobre estado limpio
	TestEqual(TEXT("Primer efecto fija Now + Duration"),
		ExtendEffectEndTime(0.f, 100.f, 3.f), 103.f);

	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Cangrejo — transición de chase
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTNCrabChaseTransitionTest,
	"Tortunabo.Enemies.Crab.ChaseTransition",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FTNCrabChaseTransitionTest::RunTest(const FString& Parameters)
{
	using namespace TNCrabLogic;

	TestTrue(TEXT("Target inválido/muerto → volver a patrol"),
		DecideChaseTransition(false, 0.f, 800.f, 0.f, 120.f)
			== EChaseTransition::ReturnToPatrol_TargetLost);

	TestTrue(TEXT("Target fuera de la zona del cangrejo → volver a patrol"),
		DecideChaseTransition(true, 800.01f, 800.f, 300.f, 120.f)
			== EChaseTransition::ReturnToPatrol_OutOfZone);

	TestTrue(TEXT("Target al alcance → atacar"),
		DecideChaseTransition(true, 400.f, 800.f, 100.f, 120.f)
			== EChaseTransition::StartAttack);

	// Borde de gameplay: EXACTAMENTE en el radio de ataque SÍ ataca (<=)
	TestTrue(TEXT("Dist == AttackRadius ataca (borde <=)"),
		DecideChaseTransition(true, 400.f, 800.f, 120.f, 120.f)
			== EChaseTransition::StartAttack);

	TestTrue(TEXT("Target vivo, en zona, lejos → seguir persiguiendo"),
		DecideChaseTransition(true, 400.f, 800.f, 300.f, 120.f)
			== EChaseTransition::KeepChasing);

	// Precedencia: target inválido gana a cualquier distancia
	TestTrue(TEXT("Target inválido tiene precedencia sobre out-of-zone"),
		DecideChaseTransition(false, 9999.f, 800.f, 0.f, 120.f)
			== EChaseTransition::ReturnToPatrol_TargetLost);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
