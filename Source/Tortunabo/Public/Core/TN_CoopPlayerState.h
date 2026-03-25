#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "TN_CoopPlayerState.generated.h"

UCLASS()
class TORTUNABO_API ATN_CoopPlayerState : public APlayerState
{
	GENERATED_BODY()

public:
	ATN_CoopPlayerState();

	bool CanServerSendQuickChat(float Now, float CooldownSeconds) const;
	void MarkServerQuickChatSent(float Now);

	bool CanServerPlayEmote(uint8 EmoteID, float Now, float CooldownSeconds) const;
	void MarkServerEmotePlayed(uint8 EmoteID, float Now);

	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Coop")
	bool bIsInReadyZone = false;

	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Coop")
	bool bHasFinishedRun = false;

	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Coop")
	bool bIsAlive = true;

	/**
	 * Down But Not Out: true when the player is incapacitated but not yet dead.
	 * Teammates can revive via emote within range. If bleedout timer expires, player dies.
	 */
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Coop|DBNO")
	bool bIsDBNO = false;

	/**
	 * Seconds remaining before the DBNO player bleeds out and dies for real.
	 * -1 = not in DBNO. Replicated to owner only (for HUD bleedout bar).
	 */
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Coop|DBNO")
	float DBNOBleedoutTimeRemaining = -1.f;

	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Coop")
	float DeathZoneTimeRemaining = -1.f;

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_EquippedHelmetId, Category = "Cosmetics")
	FName EquippedHelmetId = NAME_None;

	UFUNCTION()
	void OnRep_EquippedHelmetId();

	/** Fuerza la aplicación del casco a todos los clientes (evita la race condition del dirty-trick). */
	UFUNCTION(NetMulticast, Reliable)
	void MulticastForceApplyHelmet(FName HelmId);

	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Coop")
	float FinishTimeSeconds = -1.f;

	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Coop")
	int32 FinishRank = 0;

	/**
	 * True if this player was eliminated (killed) rather than finishing normally.
	 * UI should display "ELIMINADO" when this is true, using FinishRank for ordering
	 * among eliminated players (assigned by death order on server).
	 */
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Coop")
	bool bIsEliminated = false;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	float ServerLastQuickChatTime = -10000.f;
	TMap<uint8, float> ServerLastEmoteTimes;
};

