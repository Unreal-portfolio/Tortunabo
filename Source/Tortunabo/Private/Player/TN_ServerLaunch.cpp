#include "Player/TN_ServerLaunch.h"

// ─────────────────────────────────────────────────────────────────────────────
// FTNServerLaunch: lógica pura (servidor y cliente dueño)
// ─────────────────────────────────────────────────────────────────────────────

FVector FTNServerLaunch::Quantize(const FVector& Velocity)
{
	// FVector_NetQuantize (el parámetro del RPC) manda enteros: con la velocidad ya entera llega igual, bit a bit.
	return FVector(FMath::RoundToDouble(Velocity.X), FMath::RoundToDouble(Velocity.Y), FMath::RoundToDouble(Velocity.Z));
}

uint8 FTNServerLaunch::Grant(const FVector& Velocity, double Now, FVector& OutSentVelocity)
{
	LastGrantedId = LastGrantedId == MAX_uint8 ? 1 : LastGrantedId + 1;
	GrantedId = LastGrantedId;
	GrantedVelocity = Quantize(Velocity);
	GrantedAt = Now;
	OutSentVelocity = GrantedVelocity;
	return GrantedId;
}

bool FTNServerLaunch::Take(uint8 Id, FVector& OutVelocity)
{
	if (Id == 0 || Id != GrantedId)
	{
		return false;
	}
	GrantedId = 0;
	OutVelocity = GrantedVelocity;
	return true;
}

bool FTNServerLaunch::TakeExpired(double Now, FVector& OutVelocity)
{
	return GrantedId != 0 && Now - GrantedAt > TimeoutSeconds && Take(GrantedId, OutVelocity);
}

void FTNServerLaunch::Receive(uint8 Id, const FVector& Velocity, double Now)
{
	if (Id == 0)
	{
		return;
	}
	ClientState = EClientState::Received;
	ClientId = Id;
	ClientVelocity = Velocity;
	ClientReceivedAt = Now;
	ClientTimeStamp = -1.f;
}

bool FTNServerLaunch::BeginMove(double Now, FVector& OutVelocity)
{
	if (ClientState != EClientState::Received)
	{
		return false;
	}
	// Pasado el plazo el servidor ya la ha lanzado por su cuenta: aplicarlo ahora la lanzaría dos veces.
	if (Now - ClientReceivedAt > TimeoutSeconds)
	{
		ClientState = EClientState::None;
		return false;
	}
	ClientState = EClientState::Starting;
	OutVelocity = ClientVelocity;
	return true;
}

bool FTNServerLaunch::ConfirmMove(float TimeStamp, const FVector& Velocity)
{
	if (ClientState != EClientState::Starting)
	{
		return false;
	}
	if (!Velocity.Equals(ClientVelocity, 0.0))
	{
		ClientState = EClientState::Received;
		return false;
	}
	ClientState = EClientState::Applied;
	ClientTimeStamp = TimeStamp;
	return true;
}

void FTNServerLaunch::RetryMove()
{
	if (ClientState == EClientState::Starting)
	{
		ClientState = EClientState::Received;
	}
}

uint8 FTNServerLaunch::GetIdForMove(float TimeStamp) const
{
	return ClientState == EClientState::Applied && TimeStamp == ClientTimeStamp ? ClientId : 0;
}

bool FTNServerLaunch::GetForMove(float TimeStamp, FVector& OutVelocity) const
{
	if (GetIdForMove(TimeStamp) == 0)
	{
		return false;
	}
	OutVelocity = ClientVelocity;
	return true;
}
