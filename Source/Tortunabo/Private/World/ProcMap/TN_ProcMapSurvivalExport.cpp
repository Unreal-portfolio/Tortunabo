// Export de los mapas de Supervivencia al formato del banco (#273): un .npz por mapa, `<semilla>_<dificultad>.npz`,
// con los campos de SurvivalMap (Scripts/terrain_survival/mapa.py). Se mide con
//   cd Scripts && uv run --with matplotlib python -m terrain_survival.bench --carpeta <dir> --nombre coop --semillas 5 --hojas
// Consola (también headless con -ExecCmds): TN.Survival.Export [carpeta] [semillas=5]

#include "World/ProcMap/TN_ProcMapSurvival.h"
#include "Core/TN_Log.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace
{
	/** CRC-32 de zip (polinomio 0xEDB88320). */
	uint32 ZipCrc32(const TArray<uint8>& Data)
	{
		static uint32 Table[256];
		static bool bTable = false;
		if (!bTable)
		{
			for (uint32 i = 0; i < 256; ++i)
			{
				uint32 C = i;
				for (int32 k = 0; k < 8; ++k) { C = (C & 1u) ? 0xEDB88320u ^ (C >> 1) : C >> 1; }
				Table[i] = C;
			}
			bTable = true;
		}
		uint32 Crc = 0xFFFFFFFFu;
		for (const uint8 B : Data) { Crc = Table[(Crc ^ B) & 0xFFu] ^ (Crc >> 8); }
		return Crc ^ 0xFFFFFFFFu;
	}

	void Put16(TArray<uint8>& Out, uint32 V) { Out.Add(V & 0xFF); Out.Add((V >> 8) & 0xFF); }
	void Put32(TArray<uint8>& Out, uint32 V) { Put16(Out, V & 0xFFFF); Put16(Out, V >> 16); }

	/** Un .npy (versión 1.0): cabecera con descr y shape, y los datos en little-endian. */
	TArray<uint8> MakeNpy(const FString& Descr, const FString& Shape, const void* Data, int32 NumBytes)
	{
		FString Header = FString::Printf(TEXT("{'descr': '%s', 'fortran_order': False, 'shape': %s, }"), *Descr, *Shape);
		// Magic (6) + versión (2) + longitud (2) + cabecera terminada en \n, alineado a 64.
		const int32 Unpadded = 10 + Header.Len() + 1;
		Header += FString::ChrN((64 - Unpadded % 64) % 64, TEXT(' ')) + TEXT("\n");
		TArray<uint8> Out;
		const uint8 Magic[8] = { 0x93, 'N', 'U', 'M', 'P', 'Y', 1, 0 };
		Out.Append(Magic, 8);
		Put16(Out, Header.Len());
		const FTCHARToUTF8 Ascii(*Header);
		Out.Append(reinterpret_cast<const uint8*>(Ascii.Get()), Ascii.Length());
		Out.Append(static_cast<const uint8*>(Data), NumBytes);
		return Out;
	}

	TArray<uint8> NpyInt(int64 V) { return MakeNpy(TEXT("<i8"), TEXT("()"), &V, sizeof(V)); }
	TArray<uint8> NpyDouble(double V) { return MakeNpy(TEXT("<f8"), TEXT("()"), &V, sizeof(V)); }

	TArray<uint8> NpyString(const FString& S)
	{
		// np.savez guarda un str como '<U<n>': n caracteres en UTF-32 little-endian.
		TArray<uint32> Chars;
		for (const TCHAR C : S) { Chars.Add(static_cast<uint32>(C)); }
		return MakeNpy(FString::Printf(TEXT("<U%d"), FMath::Max(1, Chars.Num())), TEXT("()"), Chars.GetData(), Chars.Num() * 4);
	}

	/** Zip sin comprimir (método 0), que es lo que lee np.load. */
	TArray<uint8> MakeZip(const TArray<TPair<FString, TArray<uint8>>>& Files)
	{
		TArray<uint8> Out, Central;
		for (const TPair<FString, TArray<uint8>>& F : Files)
		{
			const FTCHARToUTF8 Name(*F.Key);
			const uint32 Crc = ZipCrc32(F.Value);
			const uint32 Offset = Out.Num();
			Put32(Out, 0x04034B50u); Put16(Out, 20); Put16(Out, 0); Put16(Out, 0); Put16(Out, 0); Put16(Out, 0x21);
			Put32(Out, Crc); Put32(Out, F.Value.Num()); Put32(Out, F.Value.Num()); Put16(Out, Name.Length()); Put16(Out, 0);
			Out.Append(reinterpret_cast<const uint8*>(Name.Get()), Name.Length());
			Out.Append(F.Value);

			Put32(Central, 0x02014B50u); Put16(Central, 20); Put16(Central, 20); Put16(Central, 0); Put16(Central, 0);
			Put16(Central, 0); Put16(Central, 0x21); Put32(Central, Crc); Put32(Central, F.Value.Num()); Put32(Central, F.Value.Num());
			Put16(Central, Name.Length()); Put16(Central, 0); Put16(Central, 0); Put16(Central, 0); Put16(Central, 0);
			Put32(Central, 0); Put32(Central, Offset);
			Central.Append(reinterpret_cast<const uint8*>(Name.Get()), Name.Length());
		}
		const uint32 CentralOffset = Out.Num();
		Out.Append(Central);
		Put32(Out, 0x06054B50u); Put16(Out, 0); Put16(Out, 0); Put16(Out, Files.Num()); Put16(Out, Files.Num());
		Put32(Out, Central.Num()); Put32(Out, CentralOffset); Put16(Out, 0);
		return Out;
	}

	/** Genera y exporta un mapa. Devuelve false si el generador no encontró mapa. */
	bool ExportSurvivalMap(const FString& Folder, uint32 Seed, int32 Difficulty)
	{
		using namespace TNProcMap;
		const double T0 = FPlatformTime::Seconds();
		FLayout L;
		if (!GenerateLayout(MakeSurvivalParams(Seed, Difficulty), L) || !L.bValid)
		{
			UE_LOG(LogTortunabo, Warning, TEXT("[Supervivencia] semilla %u dificultad %d: sin mapa (%hs)"), Seed, Difficulty, L.FailReason);
			return false;
		}
		FSurvivalTop Top;
		SampleSurvivalTop(L, Top);
		const double Seconds = FPlatformTime::Seconds() - T0;

		const int64 Start[2] = { Top.Start.X, Top.Start.Y };
		const int64 Goal[2] = { Top.Goal.X, Top.Goal.Y };
		TArray<TPair<FString, TArray<uint8>>> Files;
		Files.Emplace(TEXT("top.npy"), MakeNpy(TEXT("<f4"), FString::Printf(TEXT("(%d, %d)"), FSurvivalTop::Rows, FSurvivalTop::Cols),
			Top.Top.GetData(), Top.Top.Num() * sizeof(float)));
		Files.Emplace(TEXT("start.npy"), MakeNpy(TEXT("<i8"), TEXT("(2,)"), Start, sizeof(Start)));
		Files.Emplace(TEXT("goal.npy"), MakeNpy(TEXT("<i8"), TEXT("(2,)"), Goal, sizeof(Goal)));
		Files.Emplace(TEXT("seed.npy"), NpyInt(Seed));
		Files.Emplace(TEXT("difficulty.npy"), NpyInt(Difficulty));
		Files.Emplace(TEXT("algorithm.npy"), NpyString(TEXT("coop")));
		Files.Emplace(TEXT("gen_seconds.npy"), NpyDouble(Seconds));
		Files.Emplace(TEXT("triangles.npy"), NpyInt(-1));
		// Huecos de salto: una fila por hueco, (fila, columna) de cada borde y el salto más largo en metros.
		TArray<double> Jumps;
		for (const FSurvivalTop::FJump& J : Top.Jumps)
		{
			Jumps.Append({ static_cast<double>(J.From.X), static_cast<double>(J.From.Y), static_cast<double>(J.To.X),
				static_cast<double>(J.To.Y), J.LeapM });
		}
		Files.Emplace(TEXT("jumps.npy"), MakeNpy(TEXT("<f8"), FString::Printf(TEXT("(%d, 5)"), Top.Jumps.Num()),
			Jumps.GetData(), Jumps.Num() * sizeof(double)));

		const FString Path = FPaths::Combine(Folder, FString::Printf(TEXT("%u_%d.npz"), Seed, Difficulty));
		if (!FFileHelper::SaveArrayToFile(MakeZip(Files), *Path))
		{
			UE_LOG(LogTortunabo, Error, TEXT("[Supervivencia] no se pudo escribir %s"), *Path);
			return false;
		}
		return true;
	}

	FAutoConsoleCommand CmdSurvivalExport(TEXT("TN.Survival.Export"),
		TEXT("Exporta mapas de Supervivencia al formato del banco: TN.Survival.Export [carpeta] [semillas=5]. "
			"Por defecto en Saved/Supervivencia, semillas 1..N y dificultades 1..5."),
		FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
		{
			const FString Folder = Args.Num() > 0 ? Args[0] : FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Supervivencia"));
			const int32 Seeds = Args.Num() > 1 ? FMath::Max(1, FCString::Atoi(*Args[1])) : 5;
			int32 Written = 0;
			for (int32 D = TNProcMap::SurvivalMinDifficulty; D <= TNProcMap::SurvivalMaxDifficulty; ++D)
			{
				for (int32 Seed = 1; Seed <= Seeds; ++Seed)
				{
					Written += ExportSurvivalMap(Folder, static_cast<uint32>(Seed), D) ? 1 : 0;
				}
			}
			UE_LOG(LogTortunabo, Display, TEXT("[Supervivencia] %d mapas exportados en %s"), Written, *FPaths::ConvertRelativePathToFull(Folder));
		}));
}
