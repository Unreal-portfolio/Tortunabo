// Frecuencia real de la captura de voz (issue #154): TNVoiceRate (TN_VoiceRate.h), la que usa UProximityVoiceComponent
// para etiquetar lo que envía. Correr desde Session Frontend (categoría "Tortunabo.Voice") o headless:
//   UnrealEditor-Cmd <uproject> -ExecCmds="Automation RunTests Tortunabo.Voice; Quit" -nullrhi -unattended

#include "Misc/AutomationTest.h"
#include "Voice/TN_VoiceRate.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace TNVoiceRateTestHelpers
{
	/** Simula Seconds segundos de una captura que entrega RealRate muestras por segundo en bloques de 60 por segundo. */
	static int32 Feed(TNVoiceRate::FCaptureRateMeter& Meter, double& Now, double Seconds, double RealRate)
	{
		const double Step = 1.0 / 60.0;
		double Owed = 0.0;
		for (double Elapsed = 0.0; Elapsed < Seconds; Elapsed += Step)
		{
			Now += Step;
			Owed += RealRate * Step;
			const int32 Block = static_cast<int32>(Owed);
			Owed -= Block;
			Meter.Add(Block, Now);
		}
		return Meter.Rate;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTNVoiceRateSnapTest,
	"Tortunabo.Voice.SnapToStandardRate",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FTNVoiceRateSnapTest::RunTest(const FString& Parameters)
{
	using namespace TNVoiceRate;
	TestEqual(TEXT("48 000 exactos"), SnapToStandardRate(48000.0), 48000);
	TestEqual(TEXT("47 700 (hilo con prisa): 48 000"), SnapToStandardRate(47700.0), 48000);
	TestEqual(TEXT("44 000: 44 100, no 48 000"), SnapToStandardRate(44000.0), 44100);
	TestEqual(TEXT("16 200: 16 000"), SnapToStandardRate(16200.0), 16000);
	TestEqual(TEXT("24 000 (48 kHz estéreo contados como mono a la mitad)"), SnapToStandardRate(24100.0), 24000);
	TestEqual(TEXT("Nada: 0"), SnapToStandardRate(0.0), 0);
	TestEqual(TEXT("Lejos de todas (5 000): 0"), SnapToStandardRate(5000.0), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTNVoiceRateMeterTest,
	"Tortunabo.Voice.CaptureRateMeter",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FTNVoiceRateMeterTest::RunTest(const FString& Parameters)
{
	using namespace TNVoiceRate;
	using namespace TNVoiceRateTestHelpers;
	{
		// El caso de la issue: el dispositivo dice 48 kHz, pero la captura (con cancelación de eco) entrega 16 kHz.
		FCaptureRateMeter Meter;
		double Now = 100.0;
		TestEqual(TEXT("Sin medir todavía: 0"), Feed(Meter, Now, 1.0, 16000.0), 0);
		TestEqual(TEXT("Con dos ventanas de 2 s: 16 000"), Feed(Meter, Now, 5.0, 16000.0), 16000);
	}
	{
		FCaptureRateMeter Meter;
		double Now = 0.0;
		TestEqual(TEXT("48 kHz de verdad: 48 000"), Feed(Meter, Now, 6.0, 48000.0), 48000);
		TestEqual(TEXT("44,1 kHz: 44 100"), [] { FCaptureRateMeter M; double T = 0.0; return Feed(M, T, 6.0, 44100.0); }(), 44100);
	}
	{
		// Un tirón que pierde muestras en una ventana no cambia una frecuencia ya confirmada.
		FCaptureRateMeter Meter;
		double Now = 0.0;
		Feed(Meter, Now, 6.0, 48000.0);
		Feed(Meter, Now, 2.1, 20000.0);
		TestEqual(TEXT("Una ventana rara no la cambia"), Feed(Meter, Now, 2.1, 48000.0), 48000);
	}
	{
		// Sin audio (micrófono desconectado): no se inventa ninguna.
		FCaptureRateMeter Meter;
		double Now = 0.0;
		TestEqual(TEXT("Sin muestras: 0"), Feed(Meter, Now, 6.0, 0.0), 0);
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
