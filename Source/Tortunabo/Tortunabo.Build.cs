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
			"EnhancedInput",
			"OnlineSubsystem",
			"OnlineSubsystemUtils",
			"UMG",
			"Slate",
			"SlateCore",
			"AudioCapture",
			"AudioCaptureCore",
			"AudioMixer",
			"SignalProcessing",
			"Niagara",
			"NiagaraCore",
			"ProceduralMeshComponent"
		});

		// Mapa volumetrico: los trozos se convierten en StaticMesh editables (MeshDescription).
		PrivateDependencyModuleNames.AddRange(new string[] { "MeshDescription", "StaticMeshDescription" });
		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.Add("AssetRegistry");
		}

		DynamicallyLoadedModuleNames.Add("OnlineSubsystemSteam");
	}
}
