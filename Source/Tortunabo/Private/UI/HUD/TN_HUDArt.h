#pragma once

#include "CoreMinimal.h"
#include "Engine/Texture2D.h"

/**
 * Arte de boceto del HUD Tortunavy dibujado en código (para que el equipo de arte vea el estilo antes de
 * sustituirlo): iconos con aspecto de pegatina (borde crema troquelado y sombra) hechos con funciones de distancia
 * con antialias, sin texturas externas. Cada icono se dibuja una vez y queda en caché (texturas transitorias).
 *
 * Tema: tortugas que salen del nido y tienen que llegar al mar. Concha (puntos), burbujas (inventario), nube de
 * tormenta, nido con huevos (salida), olas (meta), pista de arena que acaba en el mar, cartel azul marino con una
 * ola abajo, cinta coral para nombres y caparazón hexagonal (compañeros).
 *
 * Los colores de la paleta se escriben en sRGB (hexadecimal, como en cualquier herramienta de arte) y se guardan en
 * lineal: la textura se escribe en sRGB y Slate trabaja en lineal, así que en pantalla sale el hexadecimal exacto.
 */
namespace TNHUDArt
{
	/** Color sRGB en hexadecimal (0xRRGGBB) a lineal, con alfa. */
	inline FLinearColor Hex(uint32 RGB, float Alpha = 1.f)
	{
		FLinearColor C = FLinearColor::FromSRGBColor(FColor((RGB >> 16) & 0xFF, (RGB >> 8) & 0xFF, RGB & 0xFF));
		C.A = Alpha;
		return C;
	}

	// ── Paleta Tortunavy ─────────────────────────────────────────────────────
	inline const FLinearColor Navy = Hex(0x12305A);
	inline const FLinearColor NavyDeep = Hex(0x0A1C38);
	inline const FLinearColor Ink = Hex(0x13233B);
	inline const FLinearColor Sea = Hex(0x1E9CC6);
	inline const FLinearColor SeaLight = Hex(0x62D2EA);
	inline const FLinearColor Foam = Hex(0xE0F8F5);
	inline const FLinearColor SandLight = Hex(0xFFF2D4);
	inline const FLinearColor SandC = Hex(0xF5D9A3);
	inline const FLinearColor WetSand = Hex(0xC89B5E);
	inline const FLinearColor CoralLight = Hex(0xFF9A80);
	inline const FLinearColor CoralC = Hex(0xFF6A52);
	inline const FLinearColor CoralDeep = Hex(0xD9432F);
	inline const FLinearColor Cream = Hex(0xFFFBF0);
	inline const FLinearColor Gold = Hex(0xFFCB3D);
	inline const FLinearColor ShellGreen = Hex(0x4DB35A);

	struct FPainter
	{
		int32 W = 0;
		int32 H = 0;
		TArray<FLinearColor> Px;

		/** Recorte opcional de las capas (px, inclusivo): acelera los detalles pequeños. Sin recorte si ClipX1 < 0. */
		int32 ClipX0 = 0, ClipY0 = 0, ClipX1 = -1, ClipY1 = -1;

		FPainter(int32 InW, int32 InH) : W(InW), H(InH) { Px.Init(FLinearColor(0.f, 0.f, 0.f, 0.f), W * H); }

		void Clip(float Ax, float Ay, float Bx, float By)
		{
			ClipX0 = FMath::Clamp(FMath::FloorToInt(Ax), 0, W - 1);
			ClipY0 = FMath::Clamp(FMath::FloorToInt(Ay), 0, H - 1);
			ClipX1 = FMath::Clamp(FMath::CeilToInt(Bx), 0, W - 1);
			ClipY1 = FMath::Clamp(FMath::CeilToInt(By), 0, H - 1);
		}
		void NoClip() { ClipX1 = -1; }

		/** Pinta encima (alfa normal) Color con cobertura Cov en [0, 1]. */
		void Over(int32 x, int32 y, const FLinearColor& Color, float Cov)
		{
			const float A = FMath::Clamp(Color.A * Cov, 0.f, 1.f);
			if (A <= 0.f) { return; }
			FLinearColor& D = Px[y * W + x];
			const float Out = A + D.A * (1.f - A);
			if (Out <= 1e-6f) { return; }
			D.R = (Color.R * A + D.R * D.A * (1.f - A)) / Out;
			D.G = (Color.G * A + D.G * D.A * (1.f - A)) / Out;
			D.B = (Color.B * A + D.B * D.A * (1.f - A)) / Out;
			D.A = Out;
		}

		/** Capa con antialias: Sdf(x, y) en píxeles (negativo dentro) y color por píxel. */
		template <typename FSdf, typename FColorAt>
		void Layer(const FSdf& Sdf, const FColorAt& ColorAt)
		{
			const bool bClip = ClipX1 >= 0;
			for (int32 y = bClip ? ClipY0 : 0; y <= (bClip ? ClipY1 : H - 1); ++y)
			{
				for (int32 x = bClip ? ClipX0 : 0; x <= (bClip ? ClipX1 : W - 1); ++x)
				{
					const float D = Sdf(x + 0.5f, y + 0.5f);
					const float Cov = FMath::Clamp(0.5f - D, 0.f, 1.f);
					if (Cov > 0.f) { Over(x, y, ColorAt(x + 0.5f, y + 0.5f), Cov); }
				}
			}
		}

		template <typename FSdf>
		void Fill(const FSdf& Sdf, const FLinearColor& Color)
		{
			Layer(Sdf, [&](float, float) { return Color; });
		}

		/** Pegatina: sombra difusa y borde crema de Border px alrededor de la silueta Sdf. */
		template <typename FSdf>
		void Sticker(const FSdf& Sdf, float Border, float ShadowSoft = 5.f, const FVector2f& ShadowOffset = FVector2f(2.f, 4.f))
		{
			const FLinearColor Shadow = Hex(0x0A1C38, 0.42f);
			for (int32 y = 0; y < H; ++y)
			{
				for (int32 x = 0; x < W; ++x)
				{
					const float Ds = Sdf(x + 0.5f - ShadowOffset.X, y + 0.5f - ShadowOffset.Y) - Border;
					const float Sh = FMath::Clamp(0.5f - Ds / ShadowSoft, 0.f, 1.f);
					if (Sh > 0.f) { Over(x, y, Shadow, Sh); }
				}
			}
			Fill([&](float x, float y) { return Sdf(x, y) - Border; }, Cream);
		}

		UTexture2D* ToTexture(const TCHAR* Name) const
		{
			UTexture2D* T = UTexture2D::CreateTransient(W, H, PF_B8G8R8A8, FName(Name));
			if (!T) { return nullptr; }
			T->SRGB = true;
			T->Filter = TF_Bilinear;
			T->LODGroup = TEXTUREGROUP_UI;
			FTexture2DMipMap& Mip = T->GetPlatformData()->Mips[0];
			FColor* Data = static_cast<FColor*>(Mip.BulkData.Lock(LOCK_READ_WRITE));
			for (int32 i = 0; i < W * H; ++i) { Data[i] = Px[i].ToFColor(true); }
			Mip.BulkData.Unlock();
			T->UpdateResource();
			return T;
		}
	};

	// ── Distancias con signo (en píxeles) ────────────────────────────────────
	inline float Circle(float x, float y, float Cx, float Cy, float R) { return FMath::Sqrt(FMath::Square(x - Cx) + FMath::Square(y - Cy)) - R; }

	inline float Ellipse(float x, float y, float Cx, float Cy, float Rx, float Ry)
	{
		// Aproximación: distancia en el espacio escalado por el radio menor.
		const float Nx = (x - Cx) / Rx, Ny = (y - Cy) / Ry;
		return (FMath::Sqrt(Nx * Nx + Ny * Ny) - 1.f) * FMath::Min(Rx, Ry);
	}

	inline float Box(float x, float y, float Cx, float Cy, float Hx, float Hy, float Radius)
	{
		const float Qx = FMath::Abs(x - Cx) - (Hx - Radius), Qy = FMath::Abs(y - Cy) - (Hy - Radius);
		return FMath::Sqrt(FMath::Square(FMath::Max(Qx, 0.f)) + FMath::Square(FMath::Max(Qy, 0.f))) + FMath::Min(FMath::Max(Qx, Qy), 0.f) - Radius;
	}

	inline float Segment(float x, float y, float Ax, float Ay, float Bx, float By, float R)
	{
		const float Px = x - Ax, Py = y - Ay, Dx = Bx - Ax, Dy = By - Ay;
		const float T = FMath::Clamp((Px * Dx + Py * Dy) / FMath::Max(1e-4f, Dx * Dx + Dy * Dy), 0.f, 1.f);
		return FMath::Sqrt(FMath::Square(Px - Dx * T) + FMath::Square(Py - Dy * T)) - R;
	}

	inline float Hexagon(float x, float y, float Cx, float Cy, float R)
	{
		// Hexágono con vértices arriba y abajo (caparazón visto desde arriba).
		const float Kx = -0.866025f, Ky = 0.5f, Kz = 0.577350f;
		float Px = FMath::Abs(y - Cy), Py = FMath::Abs(x - Cx);
		const float Dt = 2.f * FMath::Min(Kx * Px + Ky * Py, 0.f);
		Px -= Dt * Kx;
		Py -= Dt * Ky;
		Px -= FMath::Clamp(Px, -Kz * R, Kz * R);
		Py -= R;
		return FMath::Sqrt(Px * Px + Py * Py) * FMath::Sign(Py);
	}

	/** Polígono (puntos en orden, convexo o no): distancia con signo. */
	inline float Polygon(float x, float y, const TArray<FVector2f>& V)
	{
		float D = FLT_MAX;
		bool bInside = false;
		for (int32 i = 0, j = V.Num() - 1; i < V.Num(); j = i++)
		{
			const FVector2f E = V[j] - V[i];
			const FVector2f Q = FVector2f(x, y) - V[i];
			const float T = FMath::Clamp(FVector2f::DotProduct(Q, E) / FMath::Max(1e-4f, E.SizeSquared()), 0.f, 1.f);
			D = FMath::Min(D, (Q - E * T).Size());
			if (((V[i].Y > y) != (V[j].Y > y)) && (x < (V[j].X - V[i].X) * (y - V[i].Y) / (V[j].Y - V[i].Y) + V[i].X)) { bInside = !bInside; }
		}
		return bInside ? -D : D;
	}

	inline FLinearColor Mix(const FLinearColor& A, const FLinearColor& B, float T) { return FMath::Lerp(A, B, FMath::Clamp(T, 0.f, 1.f)); }

	/** Filo interior (línea de Width px justo por dentro del borde de Sdf), para dar volumen a las pegatinas. */
	template <typename FSdf>
	float Rim(const FSdf& Sdf, float x, float y, float Inset, float Width)
	{
		return FMath::Max(FMath::Abs(Sdf(x, y) + Inset) - Width, Sdf(x, y));
	}

	/** Arco de circunferencia de A0 a A1 (radianes, y hacia abajo) con grosor Thick y puntas redondas. */
	inline float Arc(float x, float y, float Cx, float Cy, float R, float A0, float A1, float Thick)
	{
		const float Mid = 0.5f * (A0 + A1);
		const float Half = 0.5f * (A1 - A0);
		const float D = FMath::FindDeltaAngleRadians(Mid, FMath::Atan2(y - Cy, x - Cx));
		if (FMath::Abs(D) <= Half) { return FMath::Abs(Circle(x, y, Cx, Cy, R)) - Thick; }
		const float E = D > 0.f ? A1 : A0;
		return Circle(x, y, Cx + R * FMath::Cos(E), Cy + R * FMath::Sin(E), Thick);
	}

	/** Estrella de N puntas (radio exterior R, interior R * Inner), con una punta hacia arriba. */
	inline TArray<FVector2f> StarPoints(float Cx, float Cy, float R, float Inner, int32 N = 5)
	{
		TArray<FVector2f> V;
		for (int32 i = 0; i < 2 * N; ++i)
		{
			const float A = -HALF_PI + PI * i / N;
			const float Rr = (i % 2) ? R * Inner : R;
			V.Add(FVector2f(Cx + Rr * FMath::Cos(A), Cy + Rr * FMath::Sin(A)));
		}
		return V;
	}

	/** Gota (sudor, agua): círculo con la punta hacia arriba. */
	inline float Drop(float x, float y, float Cx, float Cy, float R)
	{
		const TArray<FVector2f> Tip = { { Cx - R * 0.92f, Cy - R * 0.25f }, { Cx, Cy - R * 2.3f }, { Cx + R * 0.92f, Cy - R * 0.25f } };
		return FMath::Min(Circle(x, y, Cx, Cy, R), Polygon(x, y, Tip));
	}

	/**
	 * Caché: cada icono se dibuja una vez por ejecución. Las texturas quedan en la raíz del recolector (se reutilizan
	 * entre partidas y el motor las libera al cerrarse), como las mallas de TN_ProcMapAmbientFX.
	 */
	inline UTexture2D* Cached(FName Key, TFunctionRef<UTexture2D*()> Make)
	{
		static TMap<FName, UTexture2D*> Cache;
		if (UTexture2D** Found = Cache.Find(Key)) { return *Found; }
		UTexture2D* T = Make();
		if (T) { T->AddToRoot(); }
		Cache.Add(Key, T);
		return T;
	}

	/** Colores de una concha de vieira del HUD: degradado de arriba abajo, costillas, filo, brillo y si lleva estrella. */
	struct FShellIconPalette
	{
		FLinearColor Top;
		FLinearColor Bottom;
		FLinearColor Ribs;
		FLinearColor Rim;
		FLinearColor Gloss;
		/** Estrella de cuatro puntas arriba a la derecha (grandes y reinas). */
		bool bStar = false;
		FLinearColor Star = FLinearColor::White;
	};

	/** Concha de vieira: abanico con costillas, orejetas y brillo, con la paleta dada. */
	inline UTexture2D* PaintShellIcon(const TCHAR* TextureName, const FShellIconPalette& Pal)
	{
		FPainter P(128, 128);
		auto Body = [](float x, float y) { return FMath::Max(Circle(x, y, 64.f, 60.f, 44.f), y - 92.f); };
		auto Ears = [](float x, float y) { return Box(x, y, 64.f, 96.f, 20.f, 9.f, 4.f); };
		auto All = [&](float x, float y) { return FMath::Min(Body(x, y), Ears(x, y)); };
		const TArray<FVector2f> StarShape = StarPoints(100.f, 24.f, 18.f, 0.32f, 4);
		auto StarSdf = [&](float x, float y) { return Polygon(x, y, StarShape); };
		auto Outline = [&](float x, float y) { return Pal.bStar ? FMath::Min(All(x, y), StarSdf(x, y)) : All(x, y); };
		P.Sticker(Outline, 6.f);
		P.Layer(All, [&Pal](float x, float y) { return Mix(Pal.Top, Pal.Bottom, (y - 18.f) / 80.f); });
		// Costillas desde la charnela.
		for (int32 k = -4; k <= 4; ++k)
		{
			const float A = k * 0.3f;
			const float Ex = 64.f + FMath::Sin(A) * 50.f, Ey = 96.f - FMath::Cos(A) * 50.f;
			P.Fill([&](float x, float y) { return FMath::Max(Segment(x, y, 64.f, 96.f, Ex, Ey, 1.8f), Body(x, y)); }, Pal.Ribs);
		}
		P.Fill([&](float x, float y) { return Rim(All, x, y, 2.f, 1.4f); }, Pal.Rim);
		P.Fill([](float x, float y) { return Ellipse(x, y, 48.f, 38.f, 9.f, 6.f); }, Pal.Gloss);
		if (Pal.bStar) { P.Fill(StarSdf, Pal.Star); }
		return P.ToTexture(TextureName);
	}

	/** Concha de vieira (puntos, el contador): abanico melocotón con costillas, orejetas y brillo. */
	inline UTexture2D* ShellIcon()
	{
		return Cached(TEXT("Shell"), []
		{
			FShellIconPalette Pal;
			Pal.Top = Hex(0xFFD4BA);
			Pal.Bottom = Hex(0xFF7A5E);
			Pal.Ribs = Hex(0xC8412E, 0.55f);
			Pal.Rim = Hex(0xB8341F, 0.6f);
			Pal.Gloss = Hex(0xFFFFFF, 0.7f);
			return PaintShellIcon(TEXT("TN_HUD_Shell"), Pal);
		});
	}

	/**
	 * Concha de cada tamaño (TNScoreShells::ETier) para los iconos que vuelan al contador: pequeña dorada, normal (la
	 * del contador), grande nacarada turquesa con estrella y reina rosa y violeta con estrella dorada.
	 */
	inline UTexture2D* ShellIconTier(int32 Tier)
	{
		switch (Tier)
		{
			case 0:
				return Cached(TEXT("ShellSmall"), []
				{
					FShellIconPalette Pal;
					Pal.Top = Hex(0xFFF1B8);
					Pal.Bottom = Hex(0xF2B635);
					Pal.Ribs = Hex(0xB9801A, 0.55f);
					Pal.Rim = Hex(0xA66A10, 0.6f);
					Pal.Gloss = Hex(0xFFFFFF, 0.8f);
					return PaintShellIcon(TEXT("TN_HUD_ShellSmall"), Pal);
				});
			case 2:
				return Cached(TEXT("ShellBig"), []
				{
					FShellIconPalette Pal;
					Pal.Top = Hex(0xE6FFFB);
					Pal.Bottom = Hex(0x3CC8C8);
					Pal.Ribs = Hex(0x1A7F8A, 0.55f);
					Pal.Rim = Hex(0x146B75, 0.6f);
					Pal.Gloss = Hex(0xFFFFFF, 0.85f);
					Pal.bStar = true;
					Pal.Star = Hex(0xFFFFFF);
					return PaintShellIcon(TEXT("TN_HUD_ShellBig"), Pal);
				});
			case 3:
				return Cached(TEXT("ShellGrand"), []
				{
					FShellIconPalette Pal;
					Pal.Top = Hex(0xFFD6F5);
					Pal.Bottom = Hex(0xC04CE0);
					Pal.Ribs = Hex(0x7A1F9A, 0.55f);
					Pal.Rim = Hex(0xE8A92E, 0.9f);
					Pal.Gloss = Hex(0xFFFFFF, 0.85f);
					Pal.bStar = true;
					Pal.Star = Gold;
					return PaintShellIcon(TEXT("TN_HUD_ShellGrand"), Pal);
				});
			default:
				return ShellIcon();
		}
	}

	/** Burbuja (hueco de inventario): translúcida, filo claro y brillos. */
	inline UTexture2D* BubbleIcon()
	{
		return Cached(TEXT("Bubble"), []
		{
			FPainter P(128, 128);
			P.Fill([](float x, float y) { return Circle(x, y, 66.f, 69.f, 57.f); }, Hex(0x0A1C38, 0.3f));
			P.Layer([](float x, float y) { return Circle(x, y, 64.f, 64.f, 56.f); },
				[](float x, float y) { return Mix(Hex(0x8FE3F2, 0.6f), Hex(0x0E3D6E, 0.85f), Circle(x, y, 42.f, 38.f, 0.f) / 95.f); });
			P.Fill([](float x, float y) { return FMath::Abs(Circle(x, y, 64.f, 64.f, 54.5f)) - 2.4f; }, Hex(0xE8FFFF, 0.95f));
			P.Fill([](float x, float y) { return Ellipse(x, y, 42.f, 36.f, 15.f, 8.f); }, Hex(0xFFFFFF, 0.8f));
			P.Fill([](float x, float y) { return Circle(x, y, 86.f, 90.f, 4.f); }, Hex(0xFFFFFF, 0.55f));
			return P.ToTexture(TEXT("TN_HUD_Bubble"));
		});
	}

	/** Aro de cuerda (el hueco de la aleta): toro trenzado con filo oscuro. */
	inline UTexture2D* RopeRing()
	{
		return Cached(TEXT("Rope"), []
		{
			FPainter P(128, 128);
			auto Ring = [](float x, float y) { return FMath::Abs(Circle(x, y, 64.f, 64.f, 56.f)) - 5.5f; };
			P.Fill([](float x, float y) { return FMath::Abs(Circle(x, y, 65.f, 67.f, 56.f)) - 6.5f; }, Hex(0x0A1C38, 0.35f));
			P.Fill([&](float x, float y) { return Ring(x, y) - 1.2f; }, Hex(0x5C3B1A));
			P.Layer(Ring, [](float x, float y)
			{
				const float A = FMath::Atan2(y - 64.f, x - 64.f);
				const float R = FMath::Sqrt(FMath::Square(x - 64.f) + FMath::Square(y - 64.f));
				const float Twist = FMath::Frac((A * 9.f + (R - 56.f) * 0.18f) / PI);
				return Twist < 0.5f ? Hex(0xF2D39A) : Hex(0xA8773F);
			});
			return P.ToTexture(TEXT("TN_HUD_Rope"));
		});
	}

	/** Nube de tormenta con rayo. */
	inline UTexture2D* StormIcon()
	{
		return Cached(TEXT("Storm"), []
		{
			FPainter P(128, 104);
			auto Cloud = [](float x, float y)
			{
				float D = Circle(x, y, 44.f, 52.f, 22.f);
				D = FMath::Min(D, Circle(x, y, 68.f, 40.f, 28.f));
				D = FMath::Min(D, Circle(x, y, 92.f, 54.f, 20.f));
				return FMath::Min(D, Box(x, y, 68.f, 62.f, 40.f, 12.f, 11.f));
			};
			const TArray<FVector2f> Bolt = { { 70.f, 56.f }, { 54.f, 82.f }, { 66.f, 82.f }, { 58.f, 102.f }, { 84.f, 72.f }, { 71.f, 72.f }, { 80.f, 56.f } };
			auto BoltSdf = [&](float x, float y) { return Polygon(x, y, Bolt); };
			P.Sticker([&](float x, float y) { return FMath::Min(Cloud(x, y), BoltSdf(x, y)); }, 5.f);
			P.Layer(Cloud, [](float x, float y) { return Mix(Hex(0xA3ACC2), Hex(0x3E4459), (y - 12.f) / 62.f); });
			P.Fill([&](float x, float y) { return Rim(Cloud, x, y, 2.f, 1.3f); }, Hex(0x2A2E3E, 0.5f));
			P.Fill([&](float x, float y) { return BoltSdf(x, y) - 1.8f; }, Hex(0x3B2A08));
			P.Fill(BoltSdf, Gold);
			return P.ToTexture(TEXT("TN_HUD_Storm"));
		});
	}

	/** Nido de arena (salida): dos huevos enteros y uno roto con una tortuguita asomando. */
	inline UTexture2D* NestIcon()
	{
		return Cached(TEXT("Nest"), []
		{
			FPainter P(112, 104);
			auto Mound = [](float x, float y) { return FMath::Max(Ellipse(x, y, 56.f, 80.f, 50.f, 20.f), y - 94.f); };
			auto EggL = [](float x, float y) { return Ellipse(x, y, 30.f, 66.f, 12.f, 15.f); };
			auto EggR = [](float x, float y) { return Ellipse(x, y, 82.f, 67.f, 12.f, 15.f); };
			// Huevo roto: la mitad de abajo con el borde en zigzag.
			auto Crack = [](float x) { return 60.f + 3.5f * FMath::Abs(FMath::Frac((x - 42.f) / 7.f) * 2.f - 1.f); };
			auto Broken = [&](float x, float y) { return FMath::Max(Ellipse(x, y, 56.f, 66.f, 14.f, 17.f), Crack(x) - y); };
			auto Baby = [](float x, float y) { return Circle(x, y, 56.f, 48.f, 13.f); };
			auto All = [&](float x, float y) { return FMath::Min(FMath::Min(Mound(x, y), Baby(x, y)), FMath::Min(FMath::Min(EggL(x, y), EggR(x, y)), Broken(x, y))); };
			P.Sticker(All, 5.f);
			P.Layer(Mound, [](float x, float y) { return Mix(SandC, WetSand, (y - 64.f) / 30.f); });
			// Tortuguita: cabeza verde con ojos grandes.
			P.Fill([&](float x, float y) { return Baby(x, y) - 1.8f; }, Hex(0x1E4F26));
			P.Layer(Baby, [](float x, float y) { return Mix(Hex(0x9BE36A), Hex(0x4FA83E), (y - 36.f) / 26.f); });
			for (const float Ex : { 51.f, 62.f })
			{
				P.Fill([&](float x, float y) { return Circle(x, y, Ex, 46.f, 4.2f); }, FLinearColor::White);
				P.Fill([&](float x, float y) { return Circle(x, y, Ex + 0.8f, 47.f, 2.3f); }, Hex(0x16202A));
			}
			P.Fill([](float x, float y) { return Arc(x, y, 56.f, 50.f, 4.5f, 0.35f, 2.8f, 0.9f); }, Hex(0x1E4F26));
			for (const auto& Egg : { TFunction<float(float, float)>(EggL), TFunction<float(float, float)>(EggR), TFunction<float(float, float)>(Broken) })
			{
				P.Fill([&](float x, float y) { return Egg(x, y) - 1.5f; }, Hex(0x7A6A50));
				P.Layer(Egg, [](float x, float y) { return Mix(Hex(0xFFFDF5), Hex(0xE3DCCB), (y - 50.f) / 30.f); });
			}
			// Trocitos de cáscara en la arena.
			const TArray<FVector2f> ChipA = { { 70.f, 86.f }, { 78.f, 82.f }, { 76.f, 90.f } };
			const TArray<FVector2f> ChipB = { { 36.f, 88.f }, { 43.f, 85.f }, { 44.f, 91.f } };
			P.Fill([&](float x, float y) { return FMath::Min(Polygon(x, y, ChipA), Polygon(x, y, ChipB)); }, Hex(0xFFFDF5));
			return P.ToTexture(TEXT("TN_HUD_Nest"));
		});
	}

	/** El mar (meta): ola que rompe en rizo (con el tubo oscuro dentro), espuma, gotas y una bandera a cuadros. */
	inline UTexture2D* SeaIcon()
	{
		return Cached(TEXT("SeaIcon"), []
		{
			FPainter P(128, 104);
			auto Body = [](float x, float y) { return FMath::Min(Circle(x, y, 66.f, 50.f, 34.f), Box(x, y, 60.f, 80.f, 54.f, 16.f, 10.f)); };
			auto Barrel = [](float x, float y) { return Ellipse(x, y, 52.f, 60.f, 16.f, 14.f); };
			auto Lip = [](float x, float y) { return Circle(x, y, 33.f, 56.f, 8.f); };
			auto Pole = [](float x, float y) { return Segment(x, y, 110.f, 14.f, 110.f, 88.f, 2.6f); };
			auto Flag = [](float x, float y) { return Box(x, y, 97.f, 24.f, 12.f, 9.f, 1.f); };
			P.Sticker([&](float x, float y) { return FMath::Min(FMath::Min(Body(x, y), Lip(x, y)), FMath::Min(Pole(x, y), Flag(x, y))); }, 5.f);
			P.Layer(Body, [](float x, float y) { return Mix(Hex(0x3FC1E6), Hex(0x0F3F78), (y - 18.f) / 76.f); });
			// Tubo de la ola: hueco oscuro con un remolino de espuma.
			P.Layer([&](float x, float y) { return FMath::Max(Barrel(x, y), Body(x, y)); }, [](float x, float y) { return Mix(Hex(0x0F3F78), Hex(0x06214A), (y - 46.f) / 28.f); });
			P.Fill([](float x, float y) { return Arc(x, y, 53.f, 61.f, 7.f, -2.6f, 1.9f, 2.f); }, Hex(0x9BE4F2, 0.9f));
			// Espuma: la cresta que se curva y el labio que cae.
			P.Fill([&](float x, float y) { return FMath::Max(Arc(x, y, 64.f, 52.f, 31.f, -3.0f, -0.25f, 4.5f), Body(x, y) - 1.f); }, Foam);
			P.Fill(Lip, Foam);
			P.Fill([&](float x, float y) { return FMath::Max(FMath::Abs(y - (86.f + 3.f * FMath::Sin(x * 0.25f))) - 1.6f, Body(x, y) + 4.f); }, Hex(0x9BE4F2, 0.8f));
			P.Fill([](float x, float y) { return FMath::Min(Drop(x, y, 18.f, 44.f, 4.f), Drop(x, y, 24.f, 28.f, 3.f)); }, Hex(0x9BE4F2));
			// Bandera a cuadros de la meta.
			P.Fill(Pole, Hex(0x4A3A2A));
			P.Layer(Flag, [](float x, float y)
			{
				const int32 Cx = FMath::FloorToInt((x - 85.f) / 6.f);
				const int32 Cy = FMath::FloorToInt((y - 15.f) / 6.f);
				return ((Cx + Cy) & 1) ? Hex(0x13233B) : FLinearColor::White;
			});
			return P.ToTexture(TEXT("TN_HUD_Sea"));
		});
	}

	/** Caparazón hexagonal (marcador de un compañero): blanco para teñirlo con el color de cada jugador. */
	inline UTexture2D* ShellDotIcon()
	{
		return Cached(TEXT("ShellDot"), []
		{
			FPainter P(64, 64);
			auto Hex6 = [](float x, float y) { return Hexagon(x, y, 32.f, 32.f, 22.f); };
			P.Sticker(Hex6, 4.f, 3.f, FVector2f(1.f, 2.f));
			P.Fill(Hex6, FLinearColor::White);
			P.Fill([](float x, float y) { return FMath::Abs(Hexagon(x, y, 32.f, 32.f, 9.f)) - 1.4f; }, FLinearColor(0.f, 0.f, 0.f, 0.35f));
			for (int32 k = 0; k < 6; ++k)
			{
				const float A = PI / 6.f + k * PI / 3.f;
				P.Fill([&](float x, float y) { return Segment(x, y, 32.f + FMath::Cos(A) * 9.f, 32.f + FMath::Sin(A) * 9.f, 32.f + FMath::Cos(A) * 19.f, 32.f + FMath::Sin(A) * 19.f, 1.2f); },
					FLinearColor(0.f, 0.f, 0.f, 0.35f));
			}
			return P.ToTexture(TEXT("TN_HUD_ShellDot"));
		});
	}

	/** Pista de la playa al mar (fondo del progreso): arena seca, mojada y mar con espuma, y huellas de tortuga. */
	inline UTexture2D* TrackTexture()
	{
		return Cached(TEXT("Track"), []
		{
			FPainter P(512, 64);
			auto Band = [](float x, float y) { return Box(x, y, 256.f, 32.f, 250.f, 22.f, 20.f); };
			auto Shore = [](float y) { return 430.f + 10.f * FMath::Sin(y * 0.35f); };
			P.Sticker(Band, 4.f, 4.f, FVector2f(0.f, 3.f));
			P.Layer(Band, [&](float x, float y)
			{
				const float S = Shore(y);
				if (x > S) { return Mix(Sea, NavyDeep, (x - S) / 80.f); }
				const FLinearColor Dry = Mix(SandLight, SandC, x / 260.f);
				return Mix(Dry, WetSand, FMath::Clamp((x - 330.f) / (S - 330.f), 0.f, 1.f));
			});
			// Onditas en el mar y espuma en la orilla.
			P.Fill([&](float x, float y) { return FMath::Max(FMath::Abs(x - (Shore(y) + 22.f)) - 1.5f, Band(x, y) + 5.f); }, Hex(0x9BE4F2, 0.6f));
			P.Fill([&](float x, float y) { return FMath::Max(FMath::Abs(x - Shore(y)) - 3.f, Band(x, y)); }, Foam);
			// Huellas de tortuga: pares de marcas a lo largo de la arena.
			for (float Fx = 40.f; Fx < 405.f; Fx += 24.f)
			{
				const float Off = (FMath::RoundToInt(Fx / 24.f) % 2 == 0) ? -6.f : 6.f;
				P.Fill([&](float x, float y) { return Ellipse(x, y, Fx, 32.f + Off, 4.5f, 2.6f); }, Hex(0x9C7646, 0.5f));
			}
			P.Fill([&](float x, float y) { return Rim(Band, x, y, 1.5f, 1.2f); }, Hex(0x0A1C38, 0.25f));
			return P.ToTexture(TEXT("TN_HUD_Track"));
		});
	}

	/** Cartel azul marino con ola en el borde de abajo (se estira como caja: márgenes en TN_RunHUDWidget). */
	inline UTexture2D* CardTexture()
	{
		return Cached(TEXT("Card"), []
		{
			FPainter P(192, 128);
			auto Card = [](float x, float y)
			{
				const float Bottom = 104.f + 5.f * FMath::Sin(x * 0.2f);
				return FMath::Max(Box(x, y, 96.f, 60.f, 90.f, 54.f, 16.f), y - Bottom);
			};
			P.Sticker(Card, 4.f);
			P.Layer(Card, [](float x, float y) { return Mix(Hex(0x1A4273, 0.97f), Hex(0x0A1C38, 0.97f), y / 110.f); });
			// Brillo arriba y la ola de abajo.
			P.Fill([](float x, float y) { return FMath::Max(FMath::Abs(y - 11.f) - 1.f, Box(x, y, 96.f, 60.f, 80.f, 54.f, 16.f)); }, Hex(0xFFFFFF, 0.14f));
			P.Fill([](float x, float y) { return FMath::Max(FMath::Abs(y - (95.f + 5.f * FMath::Sin(x * 0.2f + 0.8f))) - 2.f, Box(x, y, 96.f, 60.f, 88.f, 52.f, 16.f)); },
				Hex(0x62D2EA, 0.75f));
			return P.ToTexture(TEXT("TN_HUD_Card"));
		});
	}

	/** Cinta coral con los extremos en cola de golondrina (nombres, etiquetas). */
	inline UTexture2D* RibbonTexture()
	{
		return Cached(TEXT("Ribbon"), []
		{
			FPainter P(256, 64);
			const TArray<FVector2f> Shape = { { 8.f, 14.f }, { 248.f, 14.f }, { 232.f, 32.f }, { 248.f, 50.f }, { 8.f, 50.f }, { 24.f, 32.f } };
			auto Rib = [&](float x, float y) { return Polygon(x, y, Shape); };
			P.Sticker(Rib, 4.f, 3.f, FVector2f(1.f, 3.f));
			P.Layer(Rib, [](float x, float y)
			{
				const FLinearColor C = Mix(Hex(0xFF8A70), CoralDeep, (y - 14.f) / 36.f);
				// Las colas, un poco más oscuras (como dobladas hacia atrás).
				const float Shade = (x < 30.f || x > 226.f) ? 0.78f : 1.f;
				return FLinearColor(C.R * Shade, C.G * Shade, C.B * Shade, C.A);
			});
			P.Fill([&](float x, float y) { return FMath::Max(FMath::Abs(y - 18.5f) - 1.f, Rib(x, y) + 3.f); }, Hex(0xFFFFFF, 0.35f));
			return P.ToTexture(TEXT("TN_HUD_Ribbon"));
		});
	}

	/** Etiqueta de arena (textos pequeños y puntos). */
	inline UTexture2D* SandTagTexture()
	{
		return Cached(TEXT("SandTag"), []
		{
			FPainter P(160, 56);
			auto Tag = [](float x, float y) { return Box(x, y, 80.f, 28.f, 72.f, 18.f, 14.f); };
			P.Sticker(Tag, 3.5f, 3.f, FVector2f(1.f, 2.f));
			P.Layer(Tag, [](float x, float y) { return Mix(SandLight, SandC, (y - 10.f) / 36.f); });
			P.Fill([&](float x, float y) { return Rim(Tag, x, y, 2.f, 1.f); }, Hex(0xC89B5E, 0.5f));
			return P.ToTexture(TEXT("TN_HUD_SandTag"));
		});
	}

	/**
	 * Bocadillo de voz vacío (la tortuga está hablando): crema con filo azul marino y la cola abajo a la izquierda.
	 * Dentro van unas barras de volumen que se animan en el HUD (TN_RunHUDWidget).
	 */
	inline UTexture2D* TalkBubbleTexture()
	{
		return Cached(TEXT("TalkBubble"), []
		{
			FPainter P(128, 112);
			const TArray<FVector2f> Tail = { { 26.f, 74.f }, { 10.f, 106.f }, { 52.f, 80.f } };
			auto Bubble = [&](float x, float y) { return FMath::Min(Box(x, y, 68.f, 48.f, 54.f, 38.f, 26.f), Polygon(x, y, Tail)); };
			P.Sticker(Bubble, 3.f, 4.f, FVector2f(2.f, 4.f));
			P.Fill([&](float x, float y) { return Bubble(x, y) - 1.f; }, Navy);
			P.Fill([&](float x, float y) { return Bubble(x, y) + 3.f; }, Cream);
			return P.ToTexture(TEXT("TN_HUD_TalkBubble"));
		});
	}

	/**
	 * Bocadillo de chat (frase rápida): crema con filo azul marino y la cola en la esquina de abajo a la izquierda,
	 * hacia la cara de quien habla. Se estira como caja; Slate mide los márgenes con el tamaño real de la textura, así
	 * que se dibuja pequeña (112x64) para que las esquinas y la cola no pidan más sitio que la frase. Márgenes
	 * (0.26, 0.3, 0.18, 0.45) y relleno (24, 17, 18, 24) en TN_RunHUDWidget.
	 */
	inline UTexture2D* ChatBubbleTexture()
	{
		return Cached(TEXT("ChatBubbleSmall"), []
		{
			FPainter P(112, 64);
			const TArray<FVector2f> Tail = { { 18.f, 38.f }, { 3.f, 61.f }, { 34.f, 48.f } };
			auto Bubble = [&](float x, float y) { return FMath::Min(Box(x, y, 61.f, 28.f, 48.f, 22.f, 14.f), Polygon(x, y, Tail)); };
			P.Sticker(Bubble, 2.f, 3.f, FVector2f(1.f, 2.f));
			P.Fill([&](float x, float y) { return Bubble(x, y) - 1.f; }, Navy);
			P.Fill([&](float x, float y) { return Bubble(x, y) + 3.f; }, Cream);
			return P.ToTexture(TEXT("TN_HUD_ChatBubble"));
		});
	}

	/** Ancla (adorno del cartel de estado). */
	inline UTexture2D* AnchorIcon()
	{
		return Cached(TEXT("Anchor"), []
		{
			FPainter P(64, 64);
			auto Anchor = [](float x, float y)
			{
				float D = Segment(x, y, 32.f, 14.f, 32.f, 50.f, 3.5f);
				D = FMath::Min(D, Segment(x, y, 22.f, 22.f, 42.f, 22.f, 3.f));
				D = FMath::Min(D, FMath::Abs(Circle(x, y, 32.f, 10.f, 5.f)) - 2.5f);
				D = FMath::Min(D, FMath::Max(FMath::Abs(Circle(x, y, 32.f, 36.f, 16.f)) - 3.2f, 38.f - y));
				D = FMath::Min(D, Segment(x, y, 14.f, 40.f, 18.f, 36.f, 3.f));
				return FMath::Min(D, Segment(x, y, 50.f, 40.f, 46.f, 36.f, 3.f));
			};
			P.Sticker(Anchor, 3.f, 3.f, FVector2f(1.f, 2.f));
			P.Fill(Anchor, Gold);
			return P.ToTexture(TEXT("TN_HUD_Anchor"));
		});
	}
}
