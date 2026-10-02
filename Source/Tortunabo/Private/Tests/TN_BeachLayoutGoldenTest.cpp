// Dorado del reparto de la carrera (TNBeachLayout::GenerateRound): 24 semillas × 3 dificultades con su huella (hash) y el
// hash del relieve fijo. Cualquier cambio del reparto, aunque sea de un milímetro, cambia la huella de su semilla: así un
// arreglo dice exactamente qué rondas toca y el relieve (que no depende de la ronda) se queda como está.
//
// Si un cambio del reparto es intencionado, se regenera el dorado en un commit aparte que explique por qué: el test, al
// fallar, escribe en el registro la tabla nueva lista para pegar (LogAutomationTest, «Dorado nuevo»).
// Correr: Automation RunTests Tortunabo.Beach.Golden

#include "Misc/AutomationTest.h"
#include "World/Beach/TN_BeachLayout.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace TNBeachGoldenTest
{
	constexpr int32 NumSeeds = 24;
	constexpr int32 NumDifficulties = 3;
	const ETNProcDifficulty Difficulties[NumDifficulties] = { ETNProcDifficulty::Easy, ETNProcDifficulty::Normal, ETNProcDifficulty::Hard };

	/** Las semillas del dorado: las mismas que la prueba de reglas (Tortunabo.Beach.Layout.Rules). */
	int32 SeedAt(int32 Index)
	{
		return 1000 + Index * 7919;
	}

	/** FNV-1a de 64 bits sobre enteros (las medidas, redondeadas al milímetro: la huella no depende del último bit). */
	struct FHash
	{
		uint64 Value = 1469598103934665603ull;

		void Add(int64 V)
		{
			for (int32 Byte = 0; Byte < 8; ++Byte)
			{
				Value ^= static_cast<uint64>((V >> (Byte * 8)) & 0xFF);
				Value *= 1099511628211ull;
			}
		}

		/** Una medida en cm, al milímetro. */
		void AddCm(double V) { Add(static_cast<int64>(FMath::RoundToDouble(V * 10.0))); }

		/** Un valor sin unidad (giros, escalas), a la diezmilésima. */
		void AddFine(double V) { Add(static_cast<int64>(FMath::RoundToDouble(V * 10000.0))); }

		void AddVec(const FVector2D& V)
		{
			AddCm(V.X);
			AddCm(V.Y);
		}
	};

	uint64 LayoutHash(const TNBeachLayout::FRoundLayout& L)
	{
		FHash H;
		H.Add(L.Items.Num());
		for (const TNBeachLayout::FItem& It : L.Items)
		{
			H.Add(static_cast<int64>(It.Element));
			H.AddVec(It.Pos);
			H.AddFine(It.Yaw);
			H.AddCm(It.Radius);
			H.AddCm(It.Core);
			H.AddCm(It.HalfLength);
			H.Add(It.Spec.Seed);
			H.AddFine(It.Spec.SizeScale);
			H.AddCm(It.Spec.Extent);
			H.Add(static_cast<int64>(It.Role));
			H.Add((It.bBlocking ? 1 : 0) | (It.bOverlay ? 2 : 0));
		}
		H.Add(L.Stamps.Num());
		H.Add(L.Interest.Num());
		for (const TNBeachLayout::FInterestPoint& Point : L.Interest)
		{
			H.Add(static_cast<int64>(Point.Kind));
			H.AddVec(Point.Pos);
			H.AddVec(Point.To);
			H.AddCm(Point.Height);
			H.Add(static_cast<int64>(Point.Source));
		}
		H.AddVec(L.DungeonPos);
		H.Add(L.DungeonGapSide);
		H.Add(L.bPassageOk ? 1 : 0);
		return H.Value;
	}

	/** El relieve fijo (arena, bancos, crestas, pozas, trincheras, repisa y fondo) cada 5 m, con 30-60 m de margen. */
	uint64 HeightfieldHash()
	{
		FHash H;
		for (double X = -3000.0; X <= TNBeachLayout::Length + 3000.0; X += 500.0)
		{
			for (double Y = -TNBeachLayout::HalfWidth - 6000.0; Y <= TNBeachLayout::HalfWidth + 6000.0; Y += 500.0)
			{
				H.AddCm(TNBeachLayout::SurfaceZ(X, Y));
			}
		}
		return H.Value;
	}

	/**
	 * Huellas del reparto por dificultad (Fácil, Normal, Difícil) y semilla (SeedAt). Regeneradas en un commit aparte cada
	 * vez que el reparto cambia a propósito (ver el historial del fichero).
	 */
	const uint64 GoldenLayout[NumDifficulties][NumSeeds] = {
		{ 0x42793774939F54EDull, 0x27C314569CDCCF72ull, 0x33AEEE55F81A5C99ull, 0x85283721700B5DFDull,
		  0xC17D0EA704F34B76ull, 0x07565BE3B8D47E78ull, 0x69D731A1CC55BEFEull, 0x45953F0A87C69D45ull,
		  0xCCF778F7FC244735ull, 0x343CBE250E40E38Full, 0xFE5D17336CDA15D1ull, 0xA064FA323D2266E2ull,
		  0x3F060A10F6CBB09Dull, 0x08A2093542CC4EEEull, 0xF4D0BFC2C973C69Aull, 0x2CD97BBFCAC1F35Aull,
		  0x55584CEF63CEB6F8ull, 0x8EDDCE6A4464A365ull, 0x0652305BC38A2E58ull, 0x4F83AD4FBEE5A57Bull,
		  0x77A9420D450813BEull, 0x2ADA0375C8B24668ull, 0x19D57EEA33E2BEFCull, 0x1A12605972FDE1BDull },
		{ 0xF50C0C61A148C38Eull, 0x35C82060CFDE1FC5ull, 0x6D6B64106F76F377ull, 0xB3389F08617BD720ull,
		  0x67EC36703DCAAAC3ull, 0xDD806340C014219Aull, 0x114D74D09D0259E2ull, 0x8E27607112CA8B45ull,
		  0xC392444C8C2E50DCull, 0x0EAC0E63121A55DDull, 0x9F9B70B59C8E2D7Cull, 0x3CC4AAF57891C179ull,
		  0x95A48AD27C874336ull, 0x57D7ADE80C0CC005ull, 0x84E4DA7BE30E7D67ull, 0x2C63A198225A3C9Aull,
		  0x6DFB1CE6358B952Aull, 0x72DA50B72FEE0026ull, 0x898B4319443D4B1Full, 0x6E4C7B0ED3E689BEull,
		  0xAC7C54D5EC9878FBull, 0x914E6CF9A7F0F2A7ull, 0x3D47F892511B2ABBull, 0x8817A8CDB4EDF76Aull },
		{ 0x278C6AE44EFAE7E8ull, 0x1939B02EF762B7A9ull, 0x2D856EF4235D129Cull, 0x3C9B9AEE706CA84Bull,
		  0x19A530AC7734E91Eull, 0x6139AC224559F15Eull, 0x5C48ECC9500EB894ull, 0x35750D7D76F0B126ull,
		  0x786F567637EBE086ull, 0x25B2D0CF9202063Bull, 0xEE4281821D2F7FEEull, 0x32A0C6EA809F58B1ull,
		  0xA58EBF946386C6A1ull, 0x85F5F62F70E2A7B3ull, 0xFCAE709F8F889A61ull, 0xA7075F1266C0A0FFull,
		  0x00046D50B9F7E19Aull, 0x1212F591CACEF072ull, 0xC29E7FFB0407CC4Full, 0xF694B61EB1C7A948ull,
		  0x9C4B539C0414B9A7ull, 0x9741D8EE2D1EFE62ull, 0xD6C15B012233A0E0ull, 0xB8C03308090EE0EAull },
	};

	const uint64 GoldenHeightfield = 0x760728A54DE825B7ull;

	FString FormatTable(const uint64 (&Table)[NumDifficulties][NumSeeds])
	{
		FString Out = TEXT("\n\tconst uint64 GoldenLayout[NumDifficulties][NumSeeds] = {\n");
		for (int32 d = 0; d < NumDifficulties; ++d)
		{
			Out += TEXT("\t\t{");
			for (int32 s = 0; s < NumSeeds; ++s)
			{
				Out += FString::Printf(TEXT("%s0x%016llXull"), s == 0 ? TEXT(" ") : (s % 4 == 0 ? TEXT(",\n\t\t  ") : TEXT(", ")), Table[d][s]);
			}
			Out += TEXT(" },\n");
		}
		Out += TEXT("\t};\n");
		return Out;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTNBeachLayoutGoldenTest,
	"Tortunabo.Beach.Golden.Layout",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FTNBeachLayoutGoldenTest::RunTest(const FString& Parameters)
{
	using namespace TNBeachGoldenTest;
	uint64 Actual[NumDifficulties][NumSeeds] = {};
	int32 Changed = 0;
	FString ChangedList;
	for (int32 d = 0; d < NumDifficulties; ++d)
	{
		for (int32 s = 0; s < NumSeeds; ++s)
		{
			TNBeachLayout::FRoundLayout L;
			TNBeachLayout::GenerateRound(SeedAt(s), Difficulties[d], L);
			Actual[d][s] = LayoutHash(L);
			if (Actual[d][s] != GoldenLayout[d][s])
			{
				++Changed;
				ChangedList += FString::Printf(TEXT(" %d/%d"), d, SeedAt(s));
			}
		}
	}
	if (Changed > 0)
	{
		AddError(FString::Printf(TEXT("El reparto ha cambiado en %d de %d rondas (dificultad/semilla):%s"), Changed, NumDifficulties * NumSeeds, *ChangedList));
		AddInfo(TEXT("Dorado nuevo:") + FormatTable(Actual));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTNBeachHeightfieldGoldenTest,
	"Tortunabo.Beach.Golden.Heightfield",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FTNBeachHeightfieldGoldenTest::RunTest(const FString& Parameters)
{
	// El relieve no depende de la ronda (TerrainSeed es fija): ningún cambio del reparto lo puede tocar.
	const uint64 Actual = TNBeachGoldenTest::HeightfieldHash();
	TestEqual(FString::Printf(TEXT("hash del relieve fijo (nuevo: 0x%016llXull)"), Actual), Actual, TNBeachGoldenTest::GoldenHeightfield);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
