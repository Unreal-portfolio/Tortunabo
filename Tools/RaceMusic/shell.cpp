// Arnés fuera del motor de los golpes del caparazón: compila TN_ShellImpactDSP.h (C++ puro), renderiza cada timbre a tres fuerzas
// (0,15, 0,5 y 1) en un WAV mono y escribe el pico, el nivel y la duración de cada uno. Se compila con build.bat.
//   shell <salida.wav> [rate=48000]
#define _CRT_SECURE_NO_WARNINGS
#include "TN_ShellImpactDSP.h"
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <vector>

static void WriteWav(const char* Path, const std::vector<float>& Mono, int Rate)
{
	FILE* F = fopen(Path, "wb");
	if (!F) { return; }
	const uint32_t DataBytes = static_cast<uint32_t>(Mono.size()) * 2;
	auto W32 = [&](uint32_t V) { fwrite(&V, 4, 1, F); };
	auto W16 = [&](uint16_t V) { fwrite(&V, 2, 1, F); };
	fwrite("RIFF", 1, 4, F); W32(36 + DataBytes); fwrite("WAVEfmt ", 1, 8, F);
	W32(16); W16(1); W16(1); W32(Rate); W32(Rate * 2); W16(2); W16(16);
	fwrite("data", 1, 4, F); W32(DataBytes);
	for (float S : Mono)
	{
		const int16_t V = static_cast<int16_t>(std::lrintf(std::max(-1.f, std::min(1.f, S)) * 32767.f));
		fwrite(&V, 2, 1, F);
	}
	fclose(F);
}

int main(int Argc, char** Argv)
{
	const char* Out = Argc >= 2 ? Argv[1] : "shell.wav";
	const int Rate = Argc >= 3 ? atoi(Argv[2]) : 48000;
	auto Core = std::make_unique<TNShellImpact::FCore>();
	auto Queue = std::make_unique<TNShellImpact::FQueue>();
	Core->Init(static_cast<float>(Rate));
	const char* Names[] = { "arena", "roca", "madera", "agua", "tortuga", "enemigo", "trasto" };
	const float Strengths[] = { 0.15f, 0.5f, 1.0f };
	std::vector<float> Audio;
	std::vector<float> Buf(1024);
	printf("%-8s %-6s %-9s %-9s %-9s\n", "timbre", "fuerza", "pico dBFS", "RMS dBFS", "durac(s)");
	uint32_t Seed = 7;
	for (int Kind = 0; Kind < TNShellImpact::NumKinds; ++Kind)
	{
		for (float S : Strengths)
		{
			TNShellImpact::FEvent E;
			E.Kind = static_cast<uint8_t>(Kind);
			E.Strength = S;
			E.Pitch = 1.f - 0.15f * S;
			E.Gain = 0.10f + 0.90f * std::pow(S, 1.1f);
			E.Seed = ++Seed;
			Queue->Push(E);
			size_t Start = Audio.size();
			const int Total = static_cast<int>(1.0 * Rate);
			int Done = 0;
			while (Done < Total)
			{
				Core->Render(Buf.data(), 1024, 1, *Queue);
				Audio.insert(Audio.end(), Buf.begin(), Buf.end());
				Done += 1024;
			}
			double Peak = 0, Sum = 0; int N = 0; int LastLoud = 0;
			for (size_t i = Start; i < Audio.size(); ++i)
			{
				const double A = std::fabs(Audio[i]);
				Peak = std::max(Peak, A);
				if (i - Start < static_cast<size_t>(0.3 * Rate)) { Sum += A * A; ++N; }
				if (A > 0.003) { LastLoud = static_cast<int>(i - Start); }
			}
			printf("%-8s %-6.2f %-9.1f %-9.1f %-9.2f\n", Names[Kind], S, 20 * std::log10(Peak + 1e-9), 10 * std::log10(Sum / std::max(1, N) + 1e-12), static_cast<double>(LastLoud) / Rate);
		}
	}
	WriteWav(Out, Audio, Rate);
	printf("escrito %s, voces robadas %d\n", Out, Core->GetStealCount());
	return 0;
}
