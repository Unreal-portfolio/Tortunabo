#include "Multiplayer/TN_RoomTypes.h"

namespace TNRoomCode
{
	FString Generate()
	{
		const TCHAR* Chars = Alphabet();
		const int32 Count = FCString::Strlen(Chars);
		FString Out;
		Out.Reserve(Length);
		for (int32 i = 0; i < Length; ++i)
		{
			Out.AppendChar(Chars[FMath::RandRange(0, Count - 1)]);
		}
		return Out;
	}

	bool IsAllowedChar(TCHAR Char)
	{
		return Char != TCHAR(0) && FCString::Strchr(Alphabet(), Char) != nullptr;
	}

	FString Normalize(const FString& Raw)
	{
		FString Out;
		for (const TCHAR Char : Raw)
		{
			const TCHAR Upper = FChar::ToUpper(Char);
			if (IsAllowedChar(Upper))
			{
				Out.AppendChar(Upper);
				if (Out.Len() >= Length)
				{
					break;
				}
			}
		}
		return Out;
	}

	FString FromPasted(const FString& Text)
	{
		// Primero, una palabra entera que sea un código (así «Código: K7M2P» no se queda con la C, la D y la G).
		FString Word;
		auto IsCodeWord = [](const FString& Candidate)
		{
			if (Candidate.Len() != Length)
			{
				return false;
			}
			for (const TCHAR Char : Candidate)
			{
				if (!IsAllowedChar(FChar::ToUpper(Char)))
				{
					return false;
				}
			}
			return true;
		};
		for (int32 i = 0; i <= Text.Len(); ++i)
		{
			const TCHAR Char = i < Text.Len() ? Text[i] : TCHAR(' ');
			if (FChar::IsAlnum(Char))
			{
				Word.AppendChar(Char);
				continue;
			}
			if (IsCodeWord(Word))
			{
				return Word.ToUpper();
			}
			Word.Reset();
		}
		return Normalize(Text);
	}

	bool IsComplete(const FString& Code)
	{
		return Code.Len() == Length && Normalize(Code) == Code;
	}
}

namespace TNRoomText
{
	FText Visibility(bool bPrivate)
	{
		return bPrivate ? NSLOCTEXT("TNRooms", "Private", "Privada") : NSLOCTEXT("TNRooms", "Public", "Pública");
	}

	FText RefusedMessage(const FString& Reason)
	{
		if (Reason == TNRoomKeys::RefuseLocked())
		{
			return NSLOCTEXT("TNRooms", "RefusedLocked", "La sala está cerrada: el anfitrión no deja entrar a nadie más.");
		}
		if (Reason == TNRoomKeys::RefuseFull())
		{
			return NSLOCTEXT("TNRooms", "RefusedFull", "La sala está llena: no queda sitio para otra tortuga.");
		}
		if (Reason == TNRoomKeys::RefuseKicked())
		{
			return NSLOCTEXT("TNRooms", "RefusedKicked", "El anfitrión te ha expulsado de esta sala: no puedes volver a entrar.");
		}
		return NSLOCTEXT("TNRooms", "RefusedOther", "No se ha podido entrar en la sala.");
	}
}
