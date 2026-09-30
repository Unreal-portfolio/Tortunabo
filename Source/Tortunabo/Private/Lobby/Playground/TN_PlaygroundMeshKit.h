#pragma once

#include "CoreMinimal.h"
#include "Core/TN_ProjectMaterials.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "../../World/ProcMap/TN_ProcMapRuntimeMesh.h"

/**
 * Kit de arte del parque de pruebas del lobby (ATN_JellyfishTrampoline, ATN_WobblyBridge y ATN_PlaygroundPiece) sobre los
 * buffers de TNProcMesh: colores de la paleta con el brillo en el alfa (M_CosmeticVertexColor: alfa 0 mate, más alto =
 * más metálico y pulido), cuadriláteros y triángulos con normal por vértice (superficies lisas, sin facetas), tubos,
 * elipsoides, troncos de cono, cajas orientadas, palitos de puntas redondas y adornos de playa (estrellas, conchas,
 * banderines). Los puntos de las colisiones convexas (UProceduralMeshComponent::SetCollisionConvexMeshes) salen de
 * aquí también, para que colisión y dibujo usen las mismas medidas.
 *
 * Las funciones lisas y los tubos no descartan triángulos degenerados: con la misma configuración emiten siempre el mismo
 * número de vértices, así que sirven para las mallas que se actualizan en cada fotograma (UpdateMeshSection exige la
 * misma topología). AddQuad/AddTri de FTNProcMeshBuffers sí los descartan: solo se usan en piezas rígidas, cuyas áreas
 * no cambian al moverse.
 */
namespace TNPlaygroundKit
{
	using FBuffers = TNProcMesh::FTNProcMeshBuffers;

	constexpr double KitPi = 3.14159265358979323846;
	constexpr double KitTwoPi = 6.28318530717958647692;

	/** Color sRGB 0xRRGGBB en lineal (lo que espera M_CosmeticVertexColor) con el brillo en el alfa. */
	inline FLinearColor Rgb(uint32 Hex, float Shine = 0.f)
	{
		return FLinearColor(TNProcRuntimeMesh::SRGBToLinear(((Hex >> 16) & 255) / 255.f), TNProcRuntimeMesh::SRGBToLinear(((Hex >> 8) & 255) / 255.f),
			TNProcRuntimeMesh::SRGBToLinear((Hex & 255) / 255.f), Shine);
	}

	/** Mezcla lineal de dos colores, brillo incluido. */
	inline FLinearColor Mix(const FLinearColor& A, const FLinearColor& B, double T)
	{
		const float K = static_cast<float>(FMath::Clamp(T, 0.0, 1.0));
		return FLinearColor(A.R + (B.R - A.R) * K, A.G + (B.G - A.G) * K, A.B + (B.B - A.B) * K, A.A + (B.A - A.A) * K);
	}

	/** Oscurece (< 1) o aclara (> 1) el color sin tocar el brillo. */
	inline FLinearColor Shade(const FLinearColor& Color, double Factor)
	{
		const float F = static_cast<float>(Factor);
		return FLinearColor(Color.R * F, Color.G * F, Color.B * F, Color.A);
	}

	/** El mismo color con otro brillo. */
	inline FLinearColor WithShine(const FLinearColor& Color, float Shine)
	{
		return FLinearColor(Color.R, Color.G, Color.B, Shine);
	}

	/** Colores de juguete de playa (los de las banderas del castillo): coral, turquesa, amarillo, celeste, lila, menta, rosa. */
	inline FLinearColor ToyColor(int32 Index, float Shine = 0.12f)
	{
		static const uint32 ToyHex[7] = { 0xFF6A52, 0x2EC4B6, 0xFFCB3D, 0x4CC9F0, 0x9B5DE5, 0x3DDC97, 0xFF7EB6 };
		return Rgb(ToyHex[((Index % 7) + 7) % 7], Shine);
	}

	/** Hermite 0..1 entre Edge0 y Edge1. */
	inline double Smooth01(double Edge0, double Edge1, double X)
	{
		if (Edge1 == Edge0)
		{
			return X < Edge0 ? 0.0 : 1.0;
		}
		const double T = FMath::Clamp((X - Edge0) / (Edge1 - Edge0), 0.0, 1.0);
		return T * T * (3.0 - 2.0 * T);
	}

	/** Número estable en [0, 1) por índices: posiciones y tonos iguales en todas las máquinas. */
	inline double Hash01(int32 A, int32 B, uint32 Seed)
	{
		return FMath::Clamp(0.5 * (TNProcMesh::TNProcHashNoise(A, B, Seed) + 1.0), 0.0, 0.99999);
	}

	/** Material de color de vértice del proyecto (con el del motor de respaldo). */
	inline UMaterialInterface* VertexColorMaterial()
	{
		return TNMaterials::VertexColor();
	}

	/**
	 * Malla estática en ejecución con el brillo de cada vértice (alfa de los buffers). No se guarda con el nivel ni se
	 * duplica (PIE, copiar y pegar): quien la usa la vuelve a construir en OnConstruction y en BeginPlay.
	 */
	inline UStaticMesh* BuildMesh(UObject* Outer, const FBuffers& B, UMaterialInterface* Material)
	{
		UStaticMesh* Mesh = TNProcRuntimeMesh::MakeStaticMesh(Outer, B, Material, false, 0.f, 1.5f, -2.f);
		if (Mesh)
		{
			Mesh->SetFlags(RF_DuplicateTransient);
		}
		return Mesh;
	}

	// ─────────────────────────────────────────────────────────────────────────
	// Superficies lisas
	// ─────────────────────────────────────────────────────────────────────────

	inline int32 PushVertex(FBuffers& B, const FVector& P, const FVector& N, const FLinearColor& C)
	{
		B.Verts.Add(P);
		B.Normals.Add(N);
		B.UVs.Add(FVector2D(P.X + P.Z, P.Y + P.Z) / 400.0);
		B.Colors.Add(C);
		return B.Verts.Num() - 1;
	}

	/**
	 * Triángulo con la cara frontal hacia Hint. Como FTNProcMeshBuffers::AddTri: en UE la cara frontal de (A, B, C) es la
	 * de normal (C-A)x(B-A). Nunca descarta (los degenerados no dibujan nada).
	 */
	inline void EmitOrientedTri(FBuffers& B, int32 I0, int32 I1, int32 I2, const FVector& Hint)
	{
		const FVector Face = FVector::CrossProduct(B.Verts[I1] - B.Verts[I0], B.Verts[I2] - B.Verts[I0]);
		const bool bFlip = FVector::DotProduct(Face, Hint) < 0.0;
		B.Tris.Add(I0);
		B.Tris.Add(bFlip ? I1 : I2);
		B.Tris.Add(bFlip ? I2 : I1);
	}

	/** Triángulo liso (normal y color por vértice), orientado hacia la media de las normales. */
	inline void SmoothTri(FBuffers& B, const FVector& P0, const FVector& P1, const FVector& P2, const FVector& N0, const FVector& N1, const FVector& N2,
		const FLinearColor& Color)
	{
		const int32 I0 = PushVertex(B, P0, N0, Color);
		const int32 I1 = PushVertex(B, P1, N1, Color);
		const int32 I2 = PushVertex(B, P2, N2, Color);
		EmitOrientedTri(B, I0, I1, I2, N0 + N1 + N2);
	}

	/** Cuadrilátero liso P0-P1-P2-P3 en orden de contorno, con color por vértice. */
	inline void SmoothQuad(FBuffers& B, const FVector (&P)[4], const FVector (&N)[4], const FLinearColor (&C)[4])
	{
		const int32 I0 = PushVertex(B, P[0], N[0], C[0]);
		const int32 I1 = PushVertex(B, P[1], N[1], C[1]);
		const int32 I2 = PushVertex(B, P[2], N[2], C[2]);
		const int32 I3 = PushVertex(B, P[3], N[3], C[3]);
		const FVector Hint = N[0] + N[1] + N[2] + N[3];
		EmitOrientedTri(B, I0, I1, I2, Hint);
		EmitOrientedTri(B, I0, I2, I3, Hint);
	}

	/** Cuadrilátero liso de un solo color. */
	inline void SmoothQuad1(FBuffers& B, const FVector (&P)[4], const FVector (&N)[4], const FLinearColor& Color)
	{
		const FLinearColor C[4] = { Color, Color, Color, Color };
		SmoothQuad(B, P, N, C);
	}

	// ─────────────────────────────────────────────────────────────────────────
	// Tubos (cuerdas, tentáculos, mangos)
	// ─────────────────────────────────────────────────────────────────────────

	/**
	 * Marcos a lo largo de un camino: tangente, normal transportada en paralelo (gira lo mínimo de un punto al siguiente,
	 * así el tubo no se retuerce) y binormal = tangente x normal. La normal de salida es RefUp sin su parte tangente.
	 */
	inline void PathFrames(const TArray<FVector>& Path, const FVector& RefUp, TArray<FVector>& OutT, TArray<FVector>& OutN, TArray<FVector>& OutB)
	{
		const int32 Num = Path.Num();
		OutT.SetNum(Num);
		OutN.SetNum(Num);
		OutB.SetNum(Num);
		if (Num == 0)
		{
			return;
		}
		for (int32 i = 0; i < Num; ++i)
		{
			const FVector Ahead = Path[FMath::Min(i + 1, Num - 1)];
			const FVector Behind = Path[FMath::Max(i - 1, 0)];
			FVector Tangent = Ahead - Behind;
			if (!Tangent.Normalize())
			{
				Tangent = i > 0 ? OutT[i - 1] : FVector::ForwardVector;
			}
			OutT[i] = Tangent;
		}
		FVector First = RefUp - OutT[0] * FVector::DotProduct(RefUp, OutT[0]);
		if (!First.Normalize())
		{
			First = FVector::CrossProduct(OutT[0], FMath::Abs(OutT[0].Z) < 0.9 ? FVector::UpVector : FVector::ForwardVector).GetSafeNormal();
		}
		OutN[0] = First;
		for (int32 i = 1; i < Num; ++i)
		{
			FVector Carried = OutN[i - 1];
			const FVector Turn = FVector::CrossProduct(OutT[i - 1], OutT[i]);
			const double SinA = Turn.Size();
			if (SinA > 1e-6)
			{
				const double Angle = FMath::Atan2(SinA, FVector::DotProduct(OutT[i - 1], OutT[i]));
				Carried = FQuat(Turn / SinA, Angle).RotateVector(Carried);
			}
			Carried -= OutT[i] * FVector::DotProduct(Carried, OutT[i]);
			OutN[i] = Carried.Normalize() ? Carried : OutN[i - 1];
		}
		for (int32 i = 0; i < Num; ++i)
		{
			OutB[i] = FVector::CrossProduct(OutT[i], OutN[i]);
		}
	}

	/**
	 * Tubo liso de vértices compartidos a lo largo de Path (Sides lados), con radio y color por anillo (si faltan, los
	 * últimos). Con bTipCap acaba en una punta redondeada. Topología fija: mismo número de vértices para el mismo camino.
	 */
	inline void AddTube(FBuffers& B, const TArray<FVector>& Path, const TArray<double>& Radii, int32 Sides, const TArray<FLinearColor>& RingColors,
		const FVector& RefUp, bool bTipCap)
	{
		const int32 Num = Path.Num();
		if (Num < 2 || Sides < 3)
		{
			return;
		}
		TArray<FVector> Tan, Nrm, Bin;
		PathFrames(Path, RefUp, Tan, Nrm, Bin);
		const FLinearColor Fallback = RingColors.Num() > 0 ? RingColors.Last() : FLinearColor::White;
		const int32 Base = B.Verts.Num();
		for (int32 i = 0; i < Num; ++i)
		{
			const double Rad = Radii.IsValidIndex(i) ? Radii[i] : (Radii.Num() > 0 ? Radii.Last() : 2.0);
			const FLinearColor& Col = RingColors.IsValidIndex(i) ? RingColors[i] : Fallback;
			for (int32 k = 0; k < Sides; ++k)
			{
				const double A = KitTwoPi * k / Sides;
				const FVector Radial = Nrm[i] * FMath::Cos(A) + Bin[i] * FMath::Sin(A);
				B.Verts.Add(Path[i] + Radial * Rad);
				B.Normals.Add(Radial);
				B.UVs.Add(FVector2D(static_cast<double>(k) / Sides, static_cast<double>(i)));
				B.Colors.Add(Col);
			}
		}
		// Con binormal = tangente x normal, (V(i,k), V(i+1,k), V(i,k+1)) mira hacia fuera.
		for (int32 i = 0; i + 1 < Num; ++i)
		{
			for (int32 k = 0; k < Sides; ++k)
			{
				const int32 K1 = (k + 1) % Sides;
				const int32 V00 = Base + i * Sides + k;
				const int32 V01 = Base + i * Sides + K1;
				const int32 V10 = Base + (i + 1) * Sides + k;
				const int32 V11 = Base + (i + 1) * Sides + K1;
				B.Tris.Add(V00);
				B.Tris.Add(V10);
				B.Tris.Add(V01);
				B.Tris.Add(V01);
				B.Tris.Add(V10);
				B.Tris.Add(V11);
			}
		}
		if (bTipCap)
		{
			const int32 LastRing = Num - 1;
			const double TipRad = Radii.IsValidIndex(LastRing) ? Radii[LastRing] : (Radii.Num() > 0 ? Radii.Last() : 2.0);
			const FLinearColor& TipCol = RingColors.IsValidIndex(LastRing) ? RingColors[LastRing] : Fallback;
			const int32 Tip = B.Verts.Num();
			B.Verts.Add(Path[LastRing] + Tan[LastRing] * (TipRad * 1.1));
			B.Normals.Add(Tan[LastRing]);
			B.UVs.Add(FVector2D(0.5, static_cast<double>(Num)));
			B.Colors.Add(TipCol);
			for (int32 k = 0; k < Sides; ++k)
			{
				const int32 K1 = (k + 1) % Sides;
				B.Tris.Add(Tip);
				B.Tris.Add(Base + LastRing * Sides + K1);
				B.Tris.Add(Base + LastRing * Sides + k);
			}
		}
	}

	/** Tubo recto de dos puntos con radio y color constantes. */
	inline void AddRod(FBuffers& B, const FVector& From, const FVector& To, double Radius, int32 Sides, const FLinearColor& Color, const FVector& RefUp)
	{
		const TArray<FVector> Path = { From, To };
		const TArray<double> Radii = { Radius, Radius };
		const TArray<FLinearColor> Colors = { Color, Color };
		AddTube(B, Path, Radii, Sides, Colors, RefUp, false);
	}

	// ─────────────────────────────────────────────────────────────────────────
	// Cuerpos
	// ─────────────────────────────────────────────────────────────────────────

	/** Elipsoide liso de semiejes Radii sobre los ejes ortonormales AxisX, AxisY y AxisZ. */
	inline void AddEllipsoid(FBuffers& B, const FVector& Center, const FVector& AxisX, const FVector& AxisY, const FVector& AxisZ, const FVector& Radii,
		int32 Seg, int32 Rings, const FLinearColor& Color)
	{
		const auto Sample = [&Center, &AxisX, &AxisY, &AxisZ, &Radii, Seg, Rings](int32 i, int32 j, FVector& OutP, FVector& OutN)
		{
			const double U = KitTwoPi * i / Seg;
			const double V = -0.5 * KitPi + KitPi * j / Rings;
			const double Cu = FMath::Cos(U), Su = FMath::Sin(U), Cv = FMath::Cos(V), Sv = FMath::Sin(V);
			OutP = Center + AxisX * (Radii.X * Cv * Cu) + AxisY * (Radii.Y * Cv * Su) + AxisZ * (Radii.Z * Sv);
			OutN = (AxisX * (Cv * Cu / FMath::Max(0.01, Radii.X)) + AxisY * (Cv * Su / FMath::Max(0.01, Radii.Y))
				+ AxisZ * (Sv / FMath::Max(0.01, Radii.Z))).GetSafeNormal();
		};
		for (int32 j = 0; j < Rings; ++j)
		{
			for (int32 i = 0; i < Seg; ++i)
			{
				FVector P[4], N[4];
				Sample(i, j, P[0], N[0]);
				Sample(i + 1, j, P[1], N[1]);
				Sample(i + 1, j + 1, P[2], N[2]);
				Sample(i, j + 1, P[3], N[3]);
				SmoothQuad1(B, P, N, Color);
			}
		}
	}

	/** Esfera lisa (elipsoide con los ejes del mundo). */
	inline void AddBall(FBuffers& B, const FVector& Center, double Radius, int32 Seg, const FLinearColor& Color)
	{
		AddEllipsoid(B, Center, FVector::ForwardVector, FVector::RightVector, FVector::UpVector, FVector(Radius), Seg, FMath::Max(3, Seg / 2), Color);
	}

	/** Tronco de cono liso de A a C (radios RadA y RadC) con tapas planas opcionales. */
	inline void AddFrustum(FBuffers& B, const FVector& A, const FVector& C, double RadA, double RadC, int32 Seg, const FLinearColor& SideColor,
		const FLinearColor& CapColor, bool bCapA, bool bCapC)
	{
		FVector Axis = C - A;
		const double Len = Axis.Size();
		if (Len < 0.01 || Seg < 3)
		{
			return;
		}
		Axis /= Len;
		const FVector U = FVector::CrossProduct(Axis, FMath::Abs(Axis.Z) < 0.9 ? FVector::UpVector : FVector::ForwardVector).GetSafeNormal();
		const FVector W = FVector::CrossProduct(Axis, U);
		const double Slope = (RadA - RadC) / Len;
		for (int32 k = 0; k < Seg; ++k)
		{
			const double A0 = KitTwoPi * k / Seg;
			const double A1 = KitTwoPi * (k + 1) / Seg;
			const FVector E0 = U * FMath::Cos(A0) + W * FMath::Sin(A0);
			const FVector E1 = U * FMath::Cos(A1) + W * FMath::Sin(A1);
			const FVector N0 = (E0 + Axis * Slope).GetSafeNormal();
			const FVector N1 = (E1 + Axis * Slope).GetSafeNormal();
			const FVector P[4] = { A + E0 * RadA, A + E1 * RadA, C + E1 * RadC, C + E0 * RadC };
			const FVector N[4] = { N0, N1, N1, N0 };
			SmoothQuad1(B, P, N, SideColor);
			if (bCapA && RadA > 0.01)
			{
				SmoothTri(B, A, A + E0 * RadA, A + E1 * RadA, -Axis, -Axis, -Axis, CapColor);
			}
			if (bCapC && RadC > 0.01)
			{
				SmoothTri(B, C, C + E0 * RadC, C + E1 * RadC, Axis, Axis, Axis, CapColor);
			}
		}
	}

	/** Disco plano (abanico) de normal Normal. */
	inline void AddDisc(FBuffers& B, const FVector& Center, const FVector& Normal, double Radius, int32 Seg, const FLinearColor& Color)
	{
		const FVector Nz = Normal.GetSafeNormal();
		const FVector U = FVector::CrossProduct(Nz, FMath::Abs(Nz.Z) < 0.9 ? FVector::UpVector : FVector::ForwardVector).GetSafeNormal();
		const FVector W = FVector::CrossProduct(Nz, U);
		for (int32 k = 0; k < Seg; ++k)
		{
			const double A0 = KitTwoPi * k / Seg;
			const double A1 = KitTwoPi * (k + 1) / Seg;
			SmoothTri(B, Center, Center + (U * FMath::Cos(A0) + W * FMath::Sin(A0)) * Radius, Center + (U * FMath::Cos(A1) + W * FMath::Sin(A1)) * Radius,
				Nz, Nz, Nz, Color);
		}
	}

	/** Corona plana entre RadIn y RadOut, de normal Normal. */
	inline void AddAnnulus(FBuffers& B, const FVector& Center, const FVector& Normal, double RadIn, double RadOut, int32 Seg, const FLinearColor& Color)
	{
		const FVector Nz = Normal.GetSafeNormal();
		const FVector U = FVector::CrossProduct(Nz, FMath::Abs(Nz.Z) < 0.9 ? FVector::UpVector : FVector::ForwardVector).GetSafeNormal();
		const FVector W = FVector::CrossProduct(Nz, U);
		for (int32 k = 0; k < Seg; ++k)
		{
			const double A0 = KitTwoPi * k / Seg;
			const double A1 = KitTwoPi * (k + 1) / Seg;
			const FVector E0 = U * FMath::Cos(A0) + W * FMath::Sin(A0);
			const FVector E1 = U * FMath::Cos(A1) + W * FMath::Sin(A1);
			const FVector P[4] = { Center + E0 * RadIn, Center + E1 * RadIn, Center + E1 * RadOut, Center + E0 * RadOut };
			const FVector N[4] = { Nz, Nz, Nz, Nz };
			SmoothQuad1(B, P, N, Color);
		}
	}

	/** Caja de caras planas: semiejes Half alrededor de LocalCenter en el espacio de Xf. */
	inline void AddXfBox(FBuffers& B, const FTransform& Xf, const FVector& LocalCenter, const FVector& Half, const FLinearColor& Color)
	{
		const auto Corner = [&Xf, &LocalCenter, &Half](double Sx, double Sy, double Sz)
		{
			return Xf.TransformPosition(LocalCenter + FVector(Sx * Half.X, Sy * Half.Y, Sz * Half.Z));
		};
		const FVector Ax = Xf.TransformVectorNoScale(FVector::ForwardVector);
		const FVector Ay = Xf.TransformVectorNoScale(FVector::RightVector);
		const FVector Az = Xf.TransformVectorNoScale(FVector::UpVector);
		B.AddQuad(Corner(-1, -1, 1), Corner(1, -1, 1), Corner(1, 1, 1), Corner(-1, 1, 1), Az, Color);
		B.AddQuad(Corner(-1, -1, -1), Corner(-1, 1, -1), Corner(1, 1, -1), Corner(1, -1, -1), -Az, Shade(Color, 0.8));
		B.AddQuad(Corner(1, -1, -1), Corner(1, 1, -1), Corner(1, 1, 1), Corner(1, -1, 1), Ax, Shade(Color, 0.92));
		B.AddQuad(Corner(-1, -1, -1), Corner(-1, -1, 1), Corner(-1, 1, 1), Corner(-1, 1, -1), -Ax, Shade(Color, 0.92));
		B.AddQuad(Corner(-1, 1, -1), Corner(-1, 1, 1), Corner(1, 1, 1), Corner(1, 1, -1), Ay, Shade(Color, 0.96));
		B.AddQuad(Corner(-1, -1, -1), Corner(1, -1, -1), Corner(1, -1, 1), Corner(-1, -1, 1), -Ay, Shade(Color, 0.96));
	}

	/** Caja alineada con los ejes del actor. */
	inline void AddAxisBox(FBuffers& B, const FVector& Center, const FVector& Half, const FLinearColor& Color)
	{
		AddXfBox(B, FTransform::Identity, Center, Half, Color);
	}

	/**
	 * Placa extruida de contorno convexo Outline (en el plano AxisU-AxisV, alrededor de Origin) y grosor Thick a lo largo
	 * de AxisN: palitos de helado, hojas de pala, tablas.
	 */
	inline void AddSlab(FBuffers& B, const FVector& Origin, const FVector& AxisU, const FVector& AxisV, const FVector& AxisN, const TArray<FVector2D>& Outline,
		double Thick, const FLinearColor& Color)
	{
		const int32 Num = Outline.Num();
		if (Num < 3)
		{
			return;
		}
		FVector2D Mid = FVector2D::ZeroVector;
		for (const FVector2D& Q : Outline)
		{
			Mid += Q;
		}
		Mid /= static_cast<double>(Num);
		const FVector Nz = AxisN.GetSafeNormal();
		const double Ht = Thick * 0.5;
		const auto At = [&Origin, &AxisU, &AxisV, &Nz](const FVector2D& Q, double H) { return Origin + AxisU * Q.X + AxisV * Q.Y + Nz * H; };
		for (int32 i = 0; i < Num; ++i)
		{
			const FVector2D& Q0 = Outline[i];
			const FVector2D& Q1 = Outline[(i + 1) % Num];
			B.AddTri(At(Mid, Ht), At(Q0, Ht), At(Q1, Ht), Nz, Color);
			B.AddTri(At(Mid, -Ht), At(Q0, -Ht), At(Q1, -Ht), -Nz, Shade(Color, 0.86));
			const FVector2D Out = (Q0 + Q1) * 0.5 - Mid;
			B.AddQuad(At(Q0, -Ht), At(Q1, -Ht), At(Q1, Ht), At(Q0, Ht), AxisU * Out.X + AxisV * Out.Y, Shade(Color, 0.93));
		}
	}

	/** Contorno de rectángulo de puntas redondas (estadio) de largo Length y ancho Width, centrado. */
	inline TArray<FVector2D> StadiumOutline(double Length, double Width, int32 ArcSteps = 5)
	{
		TArray<FVector2D> Outline;
		const double Rad = Width * 0.5;
		const double Straight = FMath::Max(0.0, Length * 0.5 - Rad);
		for (int32 i = 0; i <= ArcSteps; ++i)
		{
			const double A = -0.5 * KitPi + KitPi * i / ArcSteps;
			Outline.Add(FVector2D(Straight + Rad * FMath::Cos(A), Rad * FMath::Sin(A)));
		}
		for (int32 i = 0; i <= ArcSteps; ++i)
		{
			const double A = 0.5 * KitPi + KitPi * i / ArcSteps;
			Outline.Add(FVector2D(-Straight + Rad * FMath::Cos(A), Rad * FMath::Sin(A)));
		}
		return Outline;
	}

	/** Palito de helado: estadio de Length x Width y grosor Thick; AxisL a lo largo, AxisW a lo ancho. */
	inline void AddStick(FBuffers& B, const FVector& Center, const FVector& AxisL, const FVector& AxisW, double Length, double Width, double Thick,
		const FLinearColor& Color)
	{
		const FVector L = AxisL.GetSafeNormal();
		const FVector W = AxisW.GetSafeNormal();
		AddSlab(B, Center, L, W, FVector::CrossProduct(L, W), StadiumOutline(Length, Width), Thick, Color);
	}

	// ─────────────────────────────────────────────────────────────────────────
	// Adornos de playa
	// ─────────────────────────────────────────────────────────────────────────

	/** Estrella de mar abombada (cinco brazos facetados) pegada a una superficie de normal Normal. */
	inline void AddStarfish(FBuffers& B, const FVector& Center, const FVector& Normal, const FVector& Heading, double Radius, double Puff, const FLinearColor& Color)
	{
		const FVector Nz = Normal.GetSafeNormal();
		const FVector Fx = (Heading - Nz * FVector::DotProduct(Heading, Nz)).GetSafeNormal();
		const FVector Fy = FVector::CrossProduct(Nz, Fx);
		const FVector Crown = Center + Nz * Puff;
		for (int32 k = 0; k < 5; ++k)
		{
			const double A0 = KitTwoPi * k / 5.0;
			const double Am = A0 + KitPi / 5.0;
			const double A1 = A0 + KitTwoPi / 5.0;
			const FVector Tip0 = Center + (Fx * FMath::Cos(A0) + Fy * FMath::Sin(A0)) * Radius + Nz * (Puff * 0.2);
			const FVector Inner = Center + (Fx * FMath::Cos(Am) + Fy * FMath::Sin(Am)) * (Radius * 0.42) + Nz * (Puff * 0.35);
			const FVector Tip1 = Center + (Fx * FMath::Cos(A1) + Fy * FMath::Sin(A1)) * Radius + Nz * (Puff * 0.2);
			B.AddTri(Crown, Tip0, Inner, Nz, Color);
			B.AddTri(Crown, Inner, Tip1, Nz, Shade(Color, 0.88));
		}
	}

	/** Concha de vieira en abanico (siete costillas) pegada a una superficie, con la charnela en Hinge. */
	inline void AddShellFan(FBuffers& B, const FVector& Hinge, const FVector& Normal, const FVector& Up, double Radius, const FLinearColor& Color)
	{
		const FVector Nz = Normal.GetSafeNormal();
		const FVector Uy = (Up - Nz * FVector::DotProduct(Up, Nz)).GetSafeNormal();
		const FVector Ux = FVector::CrossProduct(Uy, Nz);
		constexpr int32 Ribs = 7;
		for (int32 f = 0; f < Ribs; ++f)
		{
			const double A0 = FMath::DegreesToRadians(-65.0 + 130.0 * f / Ribs);
			const double A1 = FMath::DegreesToRadians(-65.0 + 130.0 * (f + 1) / Ribs);
			const double Am = 0.5 * (A0 + A1);
			const FVector E0 = Hinge + (Uy * FMath::Cos(A0) + Ux * FMath::Sin(A0)) * Radius + Nz * 1.5;
			const FVector E1 = Hinge + (Uy * FMath::Cos(A1) + Ux * FMath::Sin(A1)) * Radius + Nz * 1.5;
			const FVector Lobe = Hinge + (Uy * FMath::Cos(Am) + Ux * FMath::Sin(Am)) * (Radius * 1.06) + Nz * 3.0;
			B.AddTri(Hinge + Nz * 2.5, E0, Lobe, Nz, (f % 2) ? Color : Shade(Color, 0.86));
			B.AddTri(Hinge + Nz * 2.5, Lobe, E1, Nz, (f % 2) ? Shade(Color, 0.93) : Color);
		}
	}

	/** Banderín triangular de dos caras colgado de PoleTop hacia Dir (horizontal). */
	inline void AddPennant(FBuffers& B, const FVector& PoleTop, const FVector& Dir, double Length, double Height, const FLinearColor& Color)
	{
		const FVector D = Dir.GetSafeNormal2D();
		const FVector Side(-D.Y, D.X, 0.0);
		const FVector Low = PoleTop - FVector(0.0, 0.0, Height);
		const FVector Tip = PoleTop + D * Length - FVector(0.0, 0.0, Height * 0.45);
		B.AddTri(PoleTop, Tip, Low, Side, Color);
		B.AddTri(PoleTop, Tip, Low, -Side, Shade(Color, 0.85));
	}

	// ─────────────────────────────────────────────────────────────────────────
	// Colisión convexa (puntos para UProceduralMeshComponent::SetCollisionConvexMeshes)
	// ─────────────────────────────────────────────────────────────────────────

	/** Las ocho esquinas de una caja en el espacio de Xf. */
	inline TArray<FVector> HullBox(const FTransform& Xf, const FVector& LocalCenter, const FVector& Half)
	{
		TArray<FVector> Pts;
		Pts.Reserve(8);
		for (int32 i = 0; i < 8; ++i)
		{
			Pts.Add(Xf.TransformPosition(LocalCenter + FVector((i & 1) ? Half.X : -Half.X, (i & 2) ? Half.Y : -Half.Y, (i & 4) ? Half.Z : -Half.Z)));
		}
		return Pts;
	}

	/** Caja alineada con los ejes del actor. */
	inline TArray<FVector> HullAxisBox(const FVector& Center, const FVector& Half)
	{
		return HullBox(FTransform::Identity, Center, Half);
	}

	/** Tronco de cono vertical de Seg lados desde Base (radios de abajo y de arriba). */
	inline TArray<FVector> HullCylinder(const FVector& Base, double Height, double RadBottom, double RadTop, int32 Seg)
	{
		TArray<FVector> Pts;
		Pts.Reserve(Seg * 2);
		for (int32 k = 0; k < Seg; ++k)
		{
			const double A = KitTwoPi * k / Seg;
			const FVector Dir(FMath::Cos(A), FMath::Sin(A), 0.0);
			Pts.Add(Base + Dir * RadBottom);
			Pts.Add(Base + Dir * RadTop + FVector(0.0, 0.0, Height));
		}
		return Pts;
	}
}
