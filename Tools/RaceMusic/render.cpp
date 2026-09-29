// Arnés fuera del motor de la música de la carrera: compila TN_RaceMusicDSP.h y TN_RaceMusicDirector.h tal cual (C++ puro) y
// renderiza WAV para oírla y medirla sin abrir el editor. Se compila con build.bat (MSVC) y se usa así:
//   render const <salida.wav> <segundos> <tension> <duck> [capas=31] [rate=48000]   pieza con parámetros fijos
//   render scenario <salida.wav> [rate=48000]                                        ronda entera: cuenta de salida, carrera, último minuto, cuenta de 10 s y silencio
//   render director                                                                  prueba de la lógica del director (sin audio)
// Variable de entorno BLOCK = tamaño del bloque de audio (1024 por defecto; probar 480 o 333 para comprobar bloques raros).
#define _CRT_SECURE_NO_WARNINGS
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <iterator>
#include <array>
#include <memory>
#include <string>
#include <vector>
#include <cstdio>
#include <cstdlib>
#include <cstring>
// Solo para el recuento de voces robadas de cada depósito (diagnóstico): abre los miembros privados del motor.
#define private public
#include "TN_RaceMusicDSP.h"
#undef private
#include "TN_RaceMusicDirector.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

static void WriteWav(const char* Path, const std::vector<float>& Interleaved, int Rate)
{
	FILE* F = fopen(Path, "wb");
	if (!F) { printf("no se puede escribir %s\n", Path); return; }
	const uint32_t NumSamples = static_cast<uint32_t>(Interleaved.size());
	const uint32_t DataBytes = NumSamples * 2;
	auto W32 = [&](uint32_t V) { fwrite(&V, 4, 1, F); };
	auto W16 = [&](uint16_t V) { fwrite(&V, 2, 1, F); };
	fwrite("RIFF", 1, 4, F); W32(36 + DataBytes); fwrite("WAVEfmt ", 1, 8, F);
	W32(16); W16(1); W16(2); W32(Rate); W32(Rate * 4); W16(4); W16(16);
	fwrite("data", 1, 4, F); W32(DataBytes);
	for (float S : Interleaved)
	{
		const float C = std::max(-1.f, std::min(1.f, S));
		const int16_t V = static_cast<int16_t>(std::lrintf(C * 32767.f));
		fwrite(&V, 2, 1, F);
	}
	fclose(F);
}

struct FEvent
{
	double T;
	int Kind; // 0 play, 1 stop, 2 tension, 3 duck, 4 fade, 5 restart, 6 volume
	float Value;
};

static void RunTimeline(const char* Out, double Seconds, int Rate, const std::vector<FEvent>& Events, uint32_t Layers)
{
	auto Engine = std::make_unique<TNRaceMusic::FEngine>();
	auto Params = std::make_unique<TNRaceMusic::FRaceMusicParams>();
	Engine->Init(static_cast<float>(Rate));
	Params->LayerMask.store(Layers);
	const int Block = getenv("BLOCK") ? atoi(getenv("BLOCK")) : 1024;
	std::vector<float> Audio;
	Audio.reserve(static_cast<size_t>(Seconds * Rate * 2));
	std::vector<float> Buf(Block * 2);
	size_t NextEvent = 0;
	int64_t Frame = 0;
	const int64_t Total = static_cast<int64_t>(Seconds * Rate);
	while (Frame < Total)
	{
		const double Now = static_cast<double>(Frame) / Rate;
		while (NextEvent < Events.size() && Events[NextEvent].T <= Now)
		{
			const FEvent& E = Events[NextEvent++];
			switch (E.Kind)
			{
			case 0: Params->bPlay.store(true); break;
			case 1: Params->bPlay.store(false); break;
			case 2: Params->Tension.store(E.Value); break;
			case 3: Params->Duck.store(E.Value); break;
			case 4: Params->FadeSeconds.store(E.Value); break;
			case 5: Params->RestartSerial.fetch_add(1); break;
			case 6: Params->Volume.store(E.Value); break;
			}
		}
		Engine->Render(Buf.data(), Block, 2, *Params);
		Audio.insert(Audio.end(), Buf.begin(), Buf.end());
		Frame += Block;
	}
	size_t Bad = 0; float MaxAbs = 0.f;
	for (float V : Audio) { if (!std::isfinite(V)) { ++Bad; } else { MaxAbs = std::max(MaxAbs, std::fabs(V)); } }
	printf("NaN/Inf: %zu, maximo abs %.3f\n", Bad, MaxAbs);
	WriteWav(Out, Audio, Rate);
	printf("escrito %s (%.1f s, %d Hz), voces robadas: %d\n", Out, Seconds, Rate, Engine->GetVoiceStealCount());
	printf("  robos: lead %d fife %d comp %d bass %d conga %d timp %d shaker %d snare %d brass %d cymbal %d orn %d bell %d\n",
		Engine->LeadVoices.StealCount, Engine->FifeVoices.StealCount, Engine->CompVoices.StealCount, Engine->BassVoices.StealCount,
		Engine->CongaVoices.StealCount, Engine->TimpVoices.StealCount, Engine->ShakerVoices.StealCount, Engine->SnareVoices.StealCount,
		Engine->BrassVoices.StealCount, Engine->CymbalVoices.StealCount, Engine->OrnVoices.StealCount, Engine->BellVoices.StealCount);
}

// ── Director ──────────────────────────────────────────────────────────────
static void PrintDecision(double T, const TNRaceMusic::FDirectorDecision& D)
{
	printf("  t=%6.2f  play=%d fade=%.1f tension=%.2f duck=%.2f  (%s)\n", T, D.bPlay ? 1 : 0, D.FadeSeconds, D.Tension, D.Duck, D.Reason);
}

static int DirectorTest()
{
	using namespace TNRaceMusic;
	FDirector Dir;
	FDirectorSnapshot S;
	S.bRaceWorld = true;
	double T = 0.0;
	auto Step = [&](double Dt, const char* Label)
	{
		T += Dt;
		S.NowSeconds = T;
		const FDirectorDecision D = Dir.Update(S);
		printf("[%s]", Label);
		PrintDecision(T, D);
		return D;
	};
	int Fails = 0;
	auto Expect = [&](bool Cond, const char* What) { if (!Cond) { printf("  FALLO: %s\n", What); ++Fails; } };

	printf("Primera ronda tras el viaje (sin cuenta numerica: huevo)\n");
	S.Phase = EPhase::Waiting; S.PhaseSecondsLeft = 0.f;
	Expect(!Step(0.1, "espera").bPlay, "no debe sonar antes de salir");
	S.Phase = EPhase::Racing;
	FDirectorDecision D = Step(0.1, "sale");
	Expect(D.bPlay && D.Duck == 0.f && D.Tension == 0.f, "carrera normal: suena sin tension ni duck");
	printf("Se pasa a la cuenta del ultimo minuto\n");
	S.RoundSecondsLeft = 60.f; D = Step(1.0, "min60"); Expect(std::fabs(D.Tension - 0.35f) < 1e-3f, "tension 0,35 a 60 s");
	S.RoundSecondsLeft = 30.f; D = Step(1.0, "min30"); Expect(std::fabs(D.Tension - 0.675f) < 1e-3f, "tension 0,675 a 30 s");
	S.RoundSecondsLeft = 0.f;  D = Step(1.0, "min0"); Expect(std::fabs(D.Tension - 1.f) < 1e-3f, "tension 1 a 0 s");
	S.RoundSecondsLeft = -1.f;
	printf("Llega la primera: cuenta de 10 s\n");
	S.Finish = EFinish::Counting; S.FinishSecondsLeft = 10.f; D = Step(1.0, "cuenta10");
	Expect(D.bPlay && D.Duck > 0.4f && D.Tension >= 0.7f, "cuenta de 10 s: aparta y sube tension");
	S.FinishSecondsLeft = 2.f; D = Step(1.0, "cuenta2");
	Expect(D.Duck > 0.6f && D.Tension > 0.9f, "ultimos 3 s: mas duck y tension casi 1");
	S.Finish = EFinish::TimeUp; D = Step(0.5, "tiempo"); Expect(!D.bPlay, "TimeUp: calla");
	S.Finish = EFinish::None; S.Phase = EPhase::RoundResults; D = Step(0.5, "recuento"); Expect(!D.bPlay, "recuento: calla");

	printf("Musica de victoria del jugador (cede)\n");
	S.Phase = EPhase::Waiting; S.PhaseSecondsLeft = 0.f; S.bOtherMusicPlaying = true;
	D = Step(0.5, "victoria"); Expect(!D.bPlay, "cede a la musica de fin de partida");
	S.bOtherMusicPlaying = false;
	D = Step(0.5, "cola"); Expect(!D.bPlay, "cola de cesion");
	S.PhaseSecondsLeft = 3.f;
	D = Step(0.5, "cuenta 3 (dentro de la cola)"); Expect(!D.bPlay, "aun cede 1,6 s");
	S.PhaseSecondsLeft = 2.f;
	D = Step(1.0, "cuenta 2"); Expect(D.bPlay && D.Duck > 0.5f, "cuenta de salida: suena apartada");
	S.PhaseSecondsLeft = 0.f;
	D = Step(0.2, "transicion"); Expect(D.bPlay, "aguanta el instante entre la cuenta y la carrera");
	S.Phase = EPhase::Racing;
	D = Step(0.2, "carrera"); Expect(D.bPlay && D.Duck == 0.f, "carrera: duck 0");
	D = Step(0.2, "carrera"); Expect(D.bPlay, "sigue");
	printf("El jugador local llega\n");
	S.bLocalFinished = true; D = Step(0.2, "llega"); Expect(!D.bPlay, "el jugador ya llego: silencio");
	S.bLocalFinished = false;
	printf("Sprint final\n");
	S.Phase = EPhase::SprintIntro; S.bSprintFinal = true; D = Step(2.0, "titulo sprint"); Expect(!D.bPlay, "titulo del sprint: silencio");
	S.Phase = EPhase::Waiting; S.PhaseSecondsLeft = 3.f; D = Step(3.0, "cuenta sprint"); Expect(D.bPlay && D.Tension >= 0.5f, "sprint: tension base");
	S.Phase = EPhase::Racing; D = Step(1.0, "sprint corre"); Expect(D.bPlay && D.Tension >= 0.5f && D.Duck == 0.f, "sprint corre con tension");
	S.Phase = EPhase::Champion; D = Step(1.0, "podio"); Expect(!D.bPlay, "podio: silencio");
	S.bRaceWorld = false; D = Step(1.0, "otro mapa"); Expect(!D.bPlay, "fuera de la carrera: silencio");
	printf("%s\n", Fails ? "DIRECTOR: FALLOS" : "DIRECTOR: OK");
	return Fails;
}

int main(int Argc, char** Argv)
{
	if (Argc < 2) { printf("uso: render const|scenario|director ...\n"); return 1; }
	const std::string Mode = Argv[1];
	if (Mode == "director") { return DirectorTest(); }
	if (Mode == "const" && Argc >= 6)
	{
		const char* Out = Argv[2];
		const double Seconds = atof(Argv[3]);
		const float Tension = static_cast<float>(atof(Argv[4]));
		const float Duck = static_cast<float>(atof(Argv[5]));
		const uint32_t Layers = Argc >= 7 ? static_cast<uint32_t>(atoi(Argv[6])) : TNRaceMusic::ELayer::All;
		const int Rate = Argc >= 8 ? atoi(Argv[7]) : 48000;
		std::vector<FEvent> Ev = { { 0.0, 4, 0.5f }, { 0.0, 2, Tension }, { 0.0, 3, Duck }, { 0.0, 0, 0.f } };
		RunTimeline(Out, Seconds, Rate, Ev, Layers);
		return 0;
	}
	if (Mode == "scenario" && Argc >= 3)
	{
		const int Rate = Argc >= 4 ? atoi(Argv[3]) : 48000;
		std::vector<FEvent> Ev = {
			{ 0.0, 4, 1.5f }, { 0.0, 3, 0.55f }, { 0.0, 0, 0.f },      // cuenta de salida apartada (con introducción)
			{ 4.5, 3, 0.0f },                                         // ¡ya!
			{ 4.5, 4, 1.2f },
			{ 60.0, 2, 0.35f }, { 70.0, 2, 0.6f }, { 80.0, 2, 0.85f }, // último minuto
			{ 90.0, 2, 0.75f }, { 92.0, 3, 0.5f },                     // llega la primera: cuenta de 10 s
			{ 95.0, 2, 1.0f }, { 97.0, 3, 0.65f },
			{ 102.0, 4, 0.8f }, { 102.0, 1, 0.f },                    // ¡TIEMPO!
			{ 112.0, 4, 1.5f }, { 112.0, 2, 0.f }, { 112.0, 3, 0.55f }, { 112.0, 0, 0.f },  // siguiente ronda
			{ 115.0, 3, 0.f },
		};
		RunTimeline(Argv[2], 150.0, Rate, Ev, TNRaceMusic::ELayer::All);
		return 0;
	}
	printf("argumentos no validos\n");
	return 1;
}
