#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Info.h"
#include "Multiplayer/TN_RoomTypes.h"
#include "TN_RoomInfo.generated.h"

/**
 * @brief La sala vista desde dentro: nombre, código, si es privada, si está cerrada y las plazas, replicado a todos.
 *
 * Lo crea la GameInstance del anfitrión en cada mapa en el que es servidor (UMP_GameInstance::RoomTick, sin tocar los
 * GameModes) y lo rellena con su sala. Los invitados lo leen para la cabecera y la página «Sala» del menú de pausa (el
 * anuncio de la sesión no se actualiza en los clientes: con el NULL nunca y con Steam, a su ritmo).
 *
 * Expulsar: el servidor apunta aquí el PlayerId del expulsado; su cliente lo ve al llegarle la réplica y se va solo al menú
 * principal con el aviso (UMP_GameInstance::HandleKickedFromRoom). Si no se va, el servidor lo echa igualmente a los pocos
 * segundos (AGameSession::KickPlayer).
 */
UCLASS(NotPlaceable, Transient)
class TORTUNABO_API ATN_RoomInfo : public AInfo
{
	GENERATED_BODY()

public:
	ATN_RoomInfo();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** El de este mundo (o nullptr si aún no ha llegado). */
	static ATN_RoomInfo* Find(const UWorld* World);

	/** Servidor: copia la sala y el nombre del anfitrión (solo replica si algo cambia). */
	void ServerApply(const FTNRoomConfig& Room, const FString& InHostName);

	/** Servidor: apunta al jugador expulsado para que su cliente se vaya. */
	void ServerMarkKicked(int32 PlayerId);

	int32 GetRoomNameId() const { return RoomNameId; }
	const FString& GetRoomCode() const { return RoomCode; }
	const FString& GetHostName() const { return HostName; }
	bool IsPrivate() const { return bPrivate; }
	bool IsLocked() const { return bLocked; }
	int32 GetMaxPlayers() const { return MaxPlayers; }

protected:
	UPROPERTY(Replicated)
	int32 RoomNameId = INDEX_NONE;

	UPROPERTY(Replicated)
	FString RoomCode;

	UPROPERTY(Replicated)
	FString HostName;

	UPROPERTY(Replicated)
	bool bPrivate = false;

	UPROPERTY(Replicated)
	bool bLocked = false;

	UPROPERTY(Replicated)
	int32 MaxPlayers = 0;

	/** PlayerId de los expulsados de este mapa. */
	UPROPERTY(ReplicatedUsing = OnRep_KickedPlayerIds)
	TArray<int32> KickedPlayerIds;

	UFUNCTION()
	void OnRep_KickedPlayerIds();
};
