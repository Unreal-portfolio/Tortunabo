// Fase 4.3 (ampliación): lógica pura del inventario de 2 slots. Sin mundo, sin
// componentes — se testean las funciones de TN_InventoryDecisions.h que
// UTN_InventoryComponent usa en producción. Correr desde Session Frontend
// (categoría "Tortunabo.Inventory") o headless:
//   UnrealEditor-Cmd <uproject> -ExecCmds="Automation RunTests Tortunabo.Inventory; Quit" -nullrhi -unattended

#include "Misc/AutomationTest.h"
#include "Player/TN_InventoryDecisions.h"

#if WITH_DEV_AUTOMATION_TESTS

// ─────────────────────────────────────────────────────────────────────────────
// Añadir — prioridad equipado > guardado
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTNInventoryAddSlotTest,
	"Tortunabo.Inventory.AddSlot",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FTNInventoryAddSlotTest::RunTest(const FString& Parameters)
{
	using namespace TNInventoryLogic;

	TestTrue(TEXT("Inventario vacío → el ítem va a la mano"),
		DecideAddSlot(false, false) == EAddDecision::ToEquipped);

	TestTrue(TEXT("Mano ocupada, caparazón libre → va al caparazón"),
		DecideAddSlot(true, false) == EAddDecision::ToStored);

	TestTrue(TEXT("Ambos slots llenos → rechazado"),
		DecideAddSlot(true, true) == EAddDecision::Rejected);

	// Estado teóricamente imposible (guardado sin equipado) — la mano libre gana
	TestTrue(TEXT("Solo el caparazón ocupado → el ítem va a la mano"),
		DecideAddSlot(false, true) == EAddDecision::ToEquipped);

	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Añadir con reemplazo
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTNInventoryAddOrReplaceTest,
	"Tortunabo.Inventory.AddOrReplace",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FTNInventoryAddOrReplaceTest::RunTest(const FString& Parameters)
{
	using namespace TNInventoryLogic;

	TestTrue(TEXT("Lleno + reemplazo permitido → reemplaza el equipado"),
		DecideAddOrReplace(true, true, true) == EAddDecision::ReplaceEquipped);

	TestTrue(TEXT("Lleno sin reemplazo → rechazado"),
		DecideAddOrReplace(true, true, false) == EAddDecision::Rejected);

	// Regla de gameplay: con hueco libre NUNCA se reemplaza (no se tira un ítem
	// pudiendo guardarlo)
	TestTrue(TEXT("Con la mano libre usa el hueco, no reemplaza"),
		DecideAddOrReplace(false, false, true) == EAddDecision::ToEquipped);
	TestTrue(TEXT("Con el caparazón libre usa el hueco, no reemplaza"),
		DecideAddOrReplace(true, false, true) == EAddDecision::ToStored);

	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// CanReceive — tabla de ocupación
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTNInventoryCanReceiveTest,
	"Tortunabo.Inventory.CanReceive",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FTNInventoryCanReceiveTest::RunTest(const FString& Parameters)
{
	using namespace TNInventoryLogic;

	TestTrue(TEXT("Vacío siempre recibe"), CanReceiveItem(false, false, false));
	TestTrue(TEXT("Con un hueco recibe aunque el reemplazo esté prohibido"),
		CanReceiveItem(true, false, false));
	TestTrue(TEXT("Lleno recibe si el reemplazo está permitido"),
		CanReceiveItem(true, true, true));
	TestFalse(TEXT("Lleno sin reemplazo no recibe"),
		CanReceiveItem(true, true, false));

	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Consumir el equipado — promoción del guardado
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTNInventoryConsumeEquippedTest,
	"Tortunabo.Inventory.ConsumeEquipped",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FTNInventoryConsumeEquippedTest::RunTest(const FString& Parameters)
{
	using namespace TNInventoryLogic;

	TestTrue(TEXT("Sin equipado no hay nada que consumir"),
		DecideConsumeEquipped(false, false) == EConsumeDecision::Rejected);

	// Regla de gameplay: al consumir la mano, el ítem del caparazón sube solo
	TestTrue(TEXT("Con guardado → consumir promociona el guardado a la mano"),
		DecideConsumeEquipped(true, true) == EConsumeDecision::ConsumeAndPromoteStored);

	TestTrue(TEXT("Sin guardado → la mano queda vacía"),
		DecideConsumeEquipped(true, false) == EConsumeDecision::ConsumeToEmpty);

	// Estado teóricamente imposible: guardado sin equipado → no se consume nada
	TestTrue(TEXT("Solo guardado sin equipado → rechazado"),
		DecideConsumeEquipped(false, true) == EConsumeDecision::Rejected);

	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Rotar slots
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTNInventorySwapSlotsTest,
	"Tortunabo.Inventory.SwapSlots",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FTNInventorySwapSlotsTest::RunTest(const FString& Parameters)
{
	using namespace TNInventoryLogic;

	TestFalse(TEXT("Ambos vacíos → rotar es no-op"), ShouldSwapSlots(false, false));

	// Rotar con un solo ítem es legal (guardarlo en el caparazón / sacarlo)
	TestTrue(TEXT("Solo equipado → rota (guarda el ítem)"), ShouldSwapSlots(true, false));
	TestTrue(TEXT("Solo guardado → rota (saca el ítem)"), ShouldSwapSlots(false, true));
	TestTrue(TEXT("Ambos ocupados → rota"), ShouldSwapSlots(true, true));

	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Consumir por UseType — precedencia equipado primero
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTNInventoryConsumeByUseTypeTest,
	"Tortunabo.Inventory.ConsumeByUseType",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FTNInventoryConsumeByUseTypeTest::RunTest(const FString& Parameters)
{
	using namespace TNInventoryLogic;

	// Precedencia: si ambos slots tienen el tipo buscado, se gasta el de la mano
	TestTrue(TEXT("Ambos matchean → gana el equipado"),
		DecideConsumeByUseType(true, true) == EUseTypeSource::Equipped);

	TestTrue(TEXT("Solo el equipado matchea → equipado"),
		DecideConsumeByUseType(true, false) == EUseTypeSource::Equipped);

	TestTrue(TEXT("Solo el guardado matchea → guardado"),
		DecideConsumeByUseType(false, true) == EUseTypeSource::Stored);

	TestTrue(TEXT("Ninguno matchea → nada que consumir"),
		DecideConsumeByUseType(false, false) == EUseTypeSource::None);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
