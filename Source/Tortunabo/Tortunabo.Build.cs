// Fill out your copyright notice in the Description page of Project Settings.

using UnrealBuildTool;

public class Tortunabo : ModuleRules
{
	public Tortunabo(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			// Material físico del caparazón con física propia (UPhysicalMaterial, FPhysicsInterface::UpdateMaterial).
			"PhysicsCore",
			"EnhancedInput",
			"OnlineSubsystem",
			"OnlineSubsystemUtils",
			"UMG",
			// Pantalla de carga del huevo durante los LoadMap bloqueantes fuera del editor.
			"MoviePlayer",
			"Slate",
			"SlateCore",
			"AudioCapture",
			"AudioCaptureCore",
			"AudioMixer",
			"SignalProcessing",
			"Niagara",
			"NiagaraCore",
			// Mapa procedural por módulos (World/ProcMap): terreno en runtime y grafos PCG por bioma.
			"ProceduralMeshComponent",
			"PCG",
			// Vegetación procedural: mallas estáticas construidas en ejecución.
			"MeshDescription",
			"StaticMeshDescription"
		});

		// Json: ATN_MapVariantLoader lee manifest.json e index.json de Scripts/terrain_volumes/Variants/.
		PrivateDependencyModuleNames.AddRange(new string[] { "Json" });
		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.Add("AssetRegistry");
		}

		DynamicallyLoadedModuleNames.Add("OnlineSubsystemSteam");
	}
}
