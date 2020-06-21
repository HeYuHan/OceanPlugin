// For copyright see LICENSE in EnvironmentProject root dir, or:
//https://github.com/UE4-OceanProject/OceanProject/blob/Master-Environment-Project/LICENSE

#pragma once

#include "CoreMinimal.h"
#include "Landscape.h"
#include "Materials/Material.h"
#include "OceanManager.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogOcean, Verbose, All);

UENUM(BlueprintType)
enum class EOceanQuality : uint8
{
	Low				UMETA(DisplayName = "Low"),
	Medium			UMETA(DisplayName = "Medium"),
	High			UMETA(DisplayName = "High"),
	VeryHigh		UMETA(DisplayName = "Very high"),
	Cinematic		UMETA(DisplayName = "Cinematic"),
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnQualityChanged, EOceanQuality, Quality);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnUnderwaterPPChange);

class UInfiniteSystemComponent;

// Which OceanMaterial to use for rendering
UENUM(BlueprintType)
enum class EOceanMaterialType : uint8
{
	OceanOld UMETA(DisplayName = "Ocean Old"),
	OceanNoSSR UMETA(DisplayName = "Ocean (No SSR)"),
	OceanWSSR UMETA(DisplayName = "Ocean w/ SSR"),
	OceanWFoamNoSSR UMETA(DisplayName = "Ocean w/ Foam (No SSR)"),
	OceanWFoamAndCubemapNoSSR UMETA(DisplayName = "Ocean w/ Foam & Cubemap (No SSR)"),
	OceanWSSRAndFoam UMETA(DisplayName = "Ocean w/ SSR & Foam"),
	OceanWSSRAndFoamAndCubemap UMETA(DisplayName = "Ocean w/ SSR, Foam & Cubemap"),
	OceanWOGerstnerNoSSR UMETA(DisplayName = "Ocean w/o Gerstner (No SSR)"),
	OceanWOGerstnerWSSR UMETA(DisplayName = "Ocean w/o Gerstner w/ SSR"),
	OceanUltra UMETA(DisplayName = "Ocean Ultra"),
	OceanStorm UMETA(DisplayName = "Ocean Storm"),
	OceanUltraforTrueSky UMETA(DisplayName = "Ocean Ultra (for TrueSky)"),
};

/*
 * Contains the parameters necessary for a single Gerstner wave.
 */
USTRUCT(BlueprintType)
struct OCEANPLUGIN_API FWaveParameter
{
	GENERATED_USTRUCT_BODY();

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
		FName WaveName;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
		float Rotation;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
		float Length;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
		float Amplitude;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
		float Steepness;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
		float TimeScale;

	FORCEINLINE FWaveParameter(FName InWaveName, float InRotation, float InLength, float InAmplitude, float InSteepness, float InTimeScale);

	// Default struct values
	FWaveParameter()
	{
		Rotation = 0.45f;
		Length = 1200.0f;
		Amplitude = 100.0f;
		Steepness = 0.8f;
		TimeScale = 1.0f;
		WaveName = FName(NAME_None);
	}
};

FORCEINLINE FWaveParameter::FWaveParameter(FName InWaveName, float InRotation, float InLength, float InAmplitude, float InSteepness, float InTimeScale)
	: WaveName(InWaveName), Rotation(InRotation), Length(InLength), Amplitude(InAmplitude), Steepness(InSteepness), TimeScale(InTimeScale)
{ }

/*
 * Contains the parameters necessary for a set of Gerstner waves.
 */
USTRUCT(BlueprintType)
struct OCEANPLUGIN_API FWaveSetParameters
{
	GENERATED_USTRUCT_BODY();

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
		FWaveParameter Wave01;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
		FWaveParameter Wave02;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
		FWaveParameter Wave03;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
		FWaveParameter Wave04;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
		FWaveParameter Wave05;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
		FWaveParameter Wave06;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
		FWaveParameter Wave07;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
		FWaveParameter Wave08;

	// Default struct values
	FWaveSetParameters()
	{
		Wave01 = FWaveParameter("Set01Wave01", 0.0f, 1.05f, 1.4f, 1.2f, 1.0f);
		Wave02 = FWaveParameter("Set01Wave02", -0.05f, 0.65f, 1.1f, 0.6f, 1.0f);
		Wave03 = FWaveParameter("Set01Wave03", 0.045f, 1.85f, 2.1f, 1.35f, 1.0f);
		Wave04 = FWaveParameter("Set01Wave04", 0.02f, 0.65f, 0.9f, 0.9f, 1.0f);
		Wave05 = FWaveParameter("Set01Wave05", -0.015f, 1.28f, 1.854f, 1.2f, 1.0f);
		Wave06 = FWaveParameter("Set01Wave06", 0.065f, 0.75f, 1.15f, 0.5f, 1.0f);
		Wave07 = FWaveParameter("Set01Wave07", 0.01f, 1.15f, 1.55f, 1.15f, 1.0f);
		Wave08 = FWaveParameter("Set01Wave08", -0.04f, 1.45f, 1.75f, 0.45f, 1.0f);
	}
};


// Cache for the "dir" variable in CalculateGerstnerWaveHeight
struct FWaveCache
{
	bool GetDir(float rotation, const FVector2D& inDirection, FVector* outDir);
	void SetDir(float rotation, const FVector2D& inDirection, const FVector& inDir);

private:
	float LastRotation = 0.0f;
	FVector2D LastDirection;
	FVector MemorizedDir;
};


/**
 * OceanManager calculates the Gerstner waves in code, while the Material uses it's own implementation in a MaterialFunction.
 * TODO: Investigate whether a single implementation could be used to increase performance.
 */
UCLASS(BlueprintType, Blueprintable)
class OCEANPLUGIN_API AOceanManager : public AActor
{
	GENERATED_BODY()

public:
	AOceanManager(const class FObjectInitializer& ObjectInitializer);

	/**
	 * Ocean static meshes.
	 * ROOT = IconComp,
	 * Visible mesh: OceanMeshComp
	 * Two invisible depth meshes: OceanMeshUnderComp, OceanMeshTopComp.
	 */

	UPROPERTY(BlueprintReadWrite)
		UBillboardComponent* IconComp;
	UPROPERTY(BlueprintReadWrite)
		UStaticMeshComponent* OceanMeshComp;
	UPROPERTY(BlueprintReadWrite)
		UStaticMeshComponent* OceanMeshUnderComp;
	UPROPERTY(BlueprintReadWrite)
		UStaticMeshComponent* OceanMeshTopComp;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
		UInfiniteSystemComponent* InfiniteSystemComp;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
		UPlanarReflectionComponent* PlanarReflectionComp;


	/**
	 * MPC parameters:
	 * 1. 	Base Material Properties (Color, Emmissive, Roughness, Metallic, Specular, Reflection, & Tesselation)
	 * 		- These Properties are NOT USED for the Depth Sorting Material
	 */

	 // The darker shade of color to be blended on the ocean surface
	UPROPERTY(Category = "Parameters|Base", BlueprintReadWrite, EditAnywhere)
		FLinearColor BaseColorDark = FLinearColor(0.0f, 0.003f, 0.01f, 1.0f);

	// The lighter shade of color to be blended on the ocean surface
	UPROPERTY(Category = "Parameters|Base", BlueprintReadWrite, EditAnywhere)
		FLinearColor BaseColorLight = FLinearColor(0.00075f, 0.021857f, 0.055f, 1.0f);

	// The color that gets blended for shallow water
	UPROPERTY(Category = "Parameters|Base", BlueprintReadWrite, EditAnywhere)
		FLinearColor ShallowWaterColor = FLinearColor(0.145f, 0.22f, 0.26f, 1.0f);

	// Controls the blending factor for the surface color
	UPROPERTY(Category = "Parameters|Base", BlueprintReadWrite, EditAnywhere)
		float BaseColorLerp = 0.85f;

	// The amount of fresnel to use in the depth fade
	UPROPERTY(Category = "Parameters|Base", BlueprintReadWrite, EditAnywhere)
		float FresnelPower = 2.0f;

	// The amount of reflection for the fresnel
	UPROPERTY(Category = "Parameters|Base", BlueprintReadWrite, EditAnywhere)
		float BaseFresnelReflect = 0.2f;

	// Controls the level of metallic on the ocean material
	UPROPERTY(Category = "Parameters|Base", BlueprintReadWrite, EditAnywhere)
		float Metallic = 0.05f;

	// Controls the roughness value on the ocean material
	UPROPERTY(Category = "Parameters|Base", BlueprintReadWrite, EditAnywhere)
		float Roughness = 0.015f;

	// Controls the specularity level for the material
	UPROPERTY(Category = "Parameters|Base", BlueprintReadWrite, EditAnywhere)
		float Specular = 1.0f;

	// Controls the amount of tesselation
	UPROPERTY(Category = "Parameters|Base", BlueprintReadWrite, EditAnywhere)
		float TesselationMultiplier = 0.8f;


	/**
	 * MPC parameters:
	 * 2. 	Base Extended Properties (Panning Normals, f, Depth Fade, & Foam)
	 * 		- These Properties are NOT USED for the Depth Sorting Material
	 */

	 // Controls the overall opacity
	UPROPERTY(Category = "Parameters|Extended Base", BlueprintReadWrite, EditAnywhere)
		float Opacity = 1.0f;

	// While looking from above, this ontrols how deep you can see under water
	UPROPERTY(Category = "Parameters|Extended Base", BlueprintReadWrite, EditAnywhere)
		float BaseDepthFade = 600.0f;

	// Controls the distortion level
	UPROPERTY(Category = "Parameters|Extended Base", BlueprintReadWrite, EditAnywhere)
		float DistortionStrength = 0.03f;

	// Controls the amount of color you can see in very shallow water
	UPROPERTY(Category = "Parameters|Extended Base", BlueprintReadWrite, EditAnywhere)
		float SceneColorCustomDepth = 10000.0f;

	// Controls the size of the foam textures used at the shoreline
	UPROPERTY(Category = "Parameters|Extended Base", BlueprintReadWrite, EditAnywhere)
		float FoamScale = 3000.0f;

	// Controls how much shore foam you ca see for foam type 1
	UPROPERTY(Category = "Parameters|Extended Base", BlueprintReadWrite, EditAnywhere)
		float FoamDepth1 = 600.0f;

	// Controls how much shore foam you ca see for foam type 2
	UPROPERTY(Category = "Parameters|Extended Base", BlueprintReadWrite, EditAnywhere)
		float FoamDepth2 = 2000.0f;

	// Contols how quickly the shore foam textures pan
	UPROPERTY(Category = "Parameters|Extended Base", BlueprintReadWrite, EditAnywhere)
		float FoamTimeScale = 1.0f;

	// Controls how hard the edges of the shore foam are
	UPROPERTY(Category = "Parameters|Extended Base", BlueprintReadWrite, EditAnywhere, meta = (UIMin = "0", UIMax = "1"))
		float FoamSoftness = 0.1;

	// Controls how hard the color change from deep to shallow is
	UPROPERTY(Category = "Parameters|Extended Base", BlueprintReadWrite, EditAnywhere, meta = (UIMin = "0", UIMax = "1"))
		float SceneDepthSoftness = 0.1f;

	// Controls how hard the color change from shallow to very shallow is
	UPROPERTY(Category = "Parameters|Extended Base", BlueprintReadWrite, EditAnywhere, meta = (UIMin = "0", UIMax = "1"))
		float BaseDepthFadeSoftness = 0.9f;


	/**
	 * MPC parameters:
	 * 3. 	SubSurface Scattering
	 */

	 // The color used for sub surface scattering at the crest of waves
	UPROPERTY(Category = "Parameters|SSS", BlueprintReadWrite, EditAnywhere)
		FLinearColor SSS_Color = FLinearColor(0.3f, 0.7f, 0.6f, 0.0f);

	// Controls the scale of the sub surface scattering
	UPROPERTY(Category = "Parameters|SSS", BlueprintReadWrite, EditAnywhere, meta = (UIMin = "0", UIMax = "0.05", ClampMin = "0"))
		float SSS_Scale = 0.01;

	// Controls the color intensity of the sub surface scattering
	UPROPERTY(Category = "Parameters|SSS", BlueprintReadWrite, EditAnywhere, meta = (UIMin = "0", UIMax = "2", ClampMin = "0"))
		float SSS_Intensity = 0.4f;

	// Controls how deeply into a wave the sub surface scattering will penetrate
	UPROPERTY(Category = "Parameters|SSS", BlueprintReadWrite, EditAnywhere, meta = (UIMin = "0", UIMax = "1000", ClampMin = "0"))
		float SSS_LightDepth = 300.0;

	// Controls how much affect the sub surface scattering has on surface normals
	UPROPERTY(Category = "Parameters|SSS", BlueprintReadWrite, EditAnywhere, meta = (UIMin = "0", UIMax = "2", ClampMin = "0"))
		float SSS_MacroNormalStrength = 0.6f;


	/**
	 * MPC parameters:
	 * 4.1	Panning Normals (old shader)
	 */

	 // OLD SHADER ONLY - Controls the blending between wave sizes
	UPROPERTY(Category = "Parameters|Panning Waves (Old Shader)", BlueprintReadWrite, EditAnywhere)
		float PanWaveLerp = 0.875f;

	// OLD SHADER ONLY - Controls the intensity of the smaller panning waves
	UPROPERTY(Category = "Parameters|Panning Waves (Old Shader)", BlueprintReadWrite, EditAnywhere)
		float PanWaveIntensity = 0.225f;

	// OLD SHADER ONLY - Controls thespeed of the smaller panning waves
	UPROPERTY(Category = "Parameters|Panning Waves (Old Shader)", BlueprintReadWrite, EditAnywhere)
		float PanWaveTimeScale = 0.85f;

	// OLD SHADER ONLY - Controls how large the small panning waves should be
	UPROPERTY(Category = "Parameters|Panning Waves (Old Shader)", BlueprintReadWrite, EditAnywhere)
		float PanWaveSize = 6800.0f;

	// OLD SHADER ONLY - Controls the first smaller wave speed
	UPROPERTY(Category = "Parameters|Panning Waves (Old Shader)", BlueprintReadWrite, EditAnywhere)
		FVector Panner01Speed = FVector(-0.015, -0.05, 0.0);

	// OLD SHADER ONLY - Controls the second smaller wave speed
	UPROPERTY(Category = "Parameters|Panning Waves (Old Shader)", BlueprintReadWrite, EditAnywhere)
		FVector Panner02Speed = FVector(0.02, -0.045, 0.0);

	// OLD SHADER ONLY - Controls the third smaller wave speed
	UPROPERTY(Category = "Parameters|Panning Waves (Old Shader)", BlueprintReadWrite, EditAnywhere)
		FVector Panner03Speed = FVector(-0.015, -0.085, 0.0);


	/**
	 * MPC parameters:
	 * 4.2	Panning Normals (old shader)
	 */

	 // OLD SHADER ONLY - Controls the scale of the texture used by the tiny surface waves
	UPROPERTY(Category = "Parameters|Panning Normals (Old Shader)", BlueprintReadWrite, EditAnywhere)
		float MacroWaveScale = 1500.0f;

	// OLD SHADER ONLY - Controls the speed of the tiny surface waves
	UPROPERTY(Category = "Parameters|Panning Normals (Old Shader)", BlueprintReadWrite, EditAnywhere)
		float MacroWaveSpeed = 1.0f;

	// OLD SHADER ONLY - Controls the size of the tiny surface waves
	UPROPERTY(Category = "Parameters|Panning Normals (Old Shader)", BlueprintReadWrite, EditAnywhere)
		float MacroWaveAmplify = 0.25f;

	/**
	 * MPC parameters:
	 * 5.	Normals
	 */

	 // Controls how large the detail normal texture
	UPROPERTY(Category = "Parameters|Normals (Detail, Medium & Far Normals)", BlueprintReadWrite, EditAnywhere)
		float DetailNormalScale = 3000.0f;

	// Controls the panning speed of the detail normal texture
	UPROPERTY(Category = "Parameters|Normals (Detail, Medium & Far Normals)", BlueprintReadWrite, EditAnywhere)
		float DetailNormalSpeed = 0.2f;

	// Controls the strength of the detail normal texture
	UPROPERTY(Category = "Parameters|Normals (Detail, Medium & Far Normals)", BlueprintReadWrite, EditAnywhere)
		float DetailNormalStrength = 0.5f;

	// Controls how large the medium normal texture
	UPROPERTY(Category = "Parameters|Normals (Detail, Medium & Far Normals)", BlueprintReadWrite, EditAnywhere)
		float MediumNormalScale = 40000.0f;

	// Controls the panning speed of the medium normal texture
	UPROPERTY(Category = "Parameters|Normals (Detail, Medium & Far Normals)", BlueprintReadWrite, EditAnywhere)
		float MediumNormalSpeed = 0.05f;

	// Controls the strength of the medium normal texture
	UPROPERTY(Category = "Parameters|Normals (Detail, Medium & Far Normals)", BlueprintReadWrite, EditAnywhere)
		float MediumNormalStrength = 0.5f;
	
	// Controls how Medium in the distance blending of the Medium normals should occur
	UPROPERTY(Category = "Parameters|Normals (Detail, Medium & Medium Normals)", BlueprintReadWrite, EditAnywhere)
		float MediumNormalBlendDistance = 400000.0f;

	// Controlls the amount of falloff for the Medium normal blending
	UPROPERTY(Category = "Parameters|Normals (Detail, Medium & Medium Normals)", BlueprintReadWrite, EditAnywhere)
		float MediumNormalBlendFalloff = 400000.0f;

	// Controls how large the far away normal texture
	UPROPERTY(Category = "Parameters|Normals (Detail, Medium & Far Normals)", BlueprintReadWrite, EditAnywhere)
		float FarNormalScale = 14000.0f;

	// Controls the panning speed of the far away normal texture
	UPROPERTY(Category = "Parameters|Normals (Detail, Medium & Far Normals)", BlueprintReadWrite, EditAnywhere)
		float FarNormalSpeed = 1.0f;

	// Controls the strength of the far away normal texture
	UPROPERTY(Category = "Parameters|Normals (Detail, Medium & Far Normals)", BlueprintReadWrite, EditAnywhere)
		float FarNormalStrength = 0.5f;

	// Controls how far in the distance blending of the far normals should occur
	UPROPERTY(Category = "Parameters|Normals (Detail, Medium & Far Normals)", BlueprintReadWrite, EditAnywhere)
		float FarNormalBlendDistance = 300000.0f;

	// Controlls the amount of falloff for the far normal blending
	UPROPERTY(Category = "Parameters|Normals (Detail, Medium & Far Normals)", BlueprintReadWrite, EditAnywhere)
		float FarNormalBlendFalloff = 300000.0f;


	/**
	 * MPC parameters:
	 * 6.	Heightmap parameters
	 */

	 // Controls the scale of the heightmap based waves texture
	UPROPERTY(Category = "Parameters|Heightmap", BlueprintReadWrite, EditAnywhere)
		float HeightmapScale = 1000.0f;

	// Controls the panning speed of the heightmap based waves
	UPROPERTY(Category = "Parameters|Heightmap", BlueprintReadWrite, EditAnywhere)
		float HeightmapSpeed = 0.3f;

	// Controls the displacement of the heightmap based waves
	UPROPERTY(Category = "Parameters|Heightmap", BlueprintReadWrite, EditAnywhere)
		float HeightmapDisplacement = 10.0f;


	/**
	 * MPC parameters:
	 * 7.1	Seafoam parameters
	 */

	 // Controls the scale of the offshore sea foam texture
	UPROPERTY(Category = "Parameters|Seafoam (Based on Heightmap)", BlueprintReadWrite, EditAnywhere)
		float SeafoamScale = 3000.0f;

	// Controls the panning speed of the offshore sea foam
	UPROPERTY(Category = "Parameters|Seafoam (Based on Heightmap)", BlueprintReadWrite, EditAnywhere)
		float SeafoamSpeed = 0.5f;

	// Controls the amount of distortion applied to the offshore sea foam texture
	UPROPERTY(Category = "Parameters|Seafoam (Based on Heightmap)", BlueprintReadWrite, EditAnywhere)
		float SeafoamDistortion = 0.01f;

	// Controls the height offset for the offshore sea foam
	UPROPERTY(Category = "Parameters|Seafoam (Based on Heightmap)", BlueprintReadWrite, EditAnywhere)
		float SeafoamHeightPower = 7.5f;

	// Controls how large a section of offshore sea foam will be
	UPROPERTY(Category = "Parameters|Seafoam (Based on Heightmap)", BlueprintReadWrite, EditAnywhere)
		float SeafoamHeightMultiply = 2500.0;


	/**
	 * MPC parameters:
	 * 7.2	Foam parameters
	 */

	 // Controls the overall opacity of the wave caps
	UPROPERTY(Category = "Parameters|Foam Wave Caps", BlueprintReadWrite, EditAnywhere)
		float FoamCapsOpacity = 0.8f;

	// Controls the minimum wave height the wave caps appear at
	UPROPERTY(Category = "Parameters|Foam Wave Caps", BlueprintReadWrite, EditAnywhere)
		float FoamCapsHeight = 120.0f;

	// Controls the size of the foam caps
	UPROPERTY(Category = "Parameters|Foam Wave Caps", BlueprintReadWrite, EditAnywhere)
		float FoamCapsPower = 4.0f;

	UPROPERTY(Category = "Parameters|Foam Wave Caps", BlueprintReadWrite, EditAnywhere)
	float FoamPannerDistance = 16093.0f;

	UPROPERTY(Category = "Parameters|Foam Wave Caps", BlueprintReadWrite, EditAnywhere)
	float FoamPannerFalloff = 11654.0f;

	// CUBEMAP MATERIALS ONLY - Controls the strength of the cubemap reflection
	UPROPERTY(Category = "Parameters|Cubemap Reflection", BlueprintReadWrite, EditAnywhere)
		float CubemapReflectionStrength = 0.3f;


	/**
	 * MPC parameters:
	 * 8.	Texture parameters
	 */

	 // The small wave normal map texture
	UPROPERTY(Category = "Parameters|Textures", BlueprintReadWrite, EditAnywhere)
		UTexture2D* SmallWaveNormal;

	// The medium wave normal map texture
	UPROPERTY(Category = "Parameters|Textures", BlueprintReadWrite, EditAnywhere)
		UTexture2D* MediumWaveNormal;

	// The far wave normal map texture
	UPROPERTY(Category = "Parameters|Textures", BlueprintReadWrite, EditAnywhere)
		UTexture2D* FarWaveNormal;

	// The primary shore foam diffuse texture
	UPROPERTY(Category = "Parameters|Textures", BlueprintReadWrite, EditAnywhere)
		UTexture2D* ShoreFoam;

	// The secondary shore foam diffuse texture
	UPROPERTY(Category = "Parameters|Textures", BlueprintReadWrite, EditAnywhere)
		UTexture2D* ShoreFoam2;

	// The roughness texture to apply to the shore foam
	UPROPERTY(Category = "Parameters|Textures", BlueprintReadWrite, EditAnywhere)
		UTexture2D* ShoreFoamRoughness;

	// These waves are used near the shoreline. 
	UPROPERTY(Category = "Parameters|Textures", BlueprintReadWrite, EditAnywhere)
		UTexture2D* HeightmapWaves;

	// The offshore foam diffuse texture
	UPROPERTY(Category = "Parameters|Textures", BlueprintReadWrite, EditAnywhere)
		UTexture2D* Seafoam;

	// CUBEMAP MATERIALS ONLY - The reflection cubemap texture
	UPROPERTY(Category = "Parameters|Textures", BlueprintReadWrite, EditAnywhere)
		UTexture2D* ReflectionCubemap;


	/**
	 * Other params (Material instances, Arrays, etc)
	 */

	 /**
	  * Actors not visible for planar reflections.
	  * This actor is added by default.
	  */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
		TArray<AActor*> HiddenActors;

	// Default ocean shader.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
		EOceanMaterialType OceanShader = EOceanMaterialType::OceanUltra;

	// The ocean dynamic material instance
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
		UMaterialInstanceDynamic* MID_Ocean;

	// The depth plane dynamic material instance
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
		UMaterialInstanceDynamic* MID_Ocean_Depth;



	UPROPERTY(BlueprintReadWrite, EditAnywhere)
		bool EnableGerstnerWaves = true;

	// The global direction the waves travel.
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
		FVector2D GlobalWaveDirection = FVector2D(0, 1);;

	// The global speed multiplier of the waves.
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
		float GlobalWaveSpeed = 2.0f;

	// The global amplitude multiplier of the waves.
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
		float GlobalWaveAmplitude = 1.0f;

	/* Optimization:
	 * If the distance of a buoyant point to base sea level exceeds DistanceCheck,
	 * skip the Gerstner calculations and return base sea level.
	 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
		float DistanceCheck = 10000.0f;

	/* Median Gerstner wave settings
	 * (only 1 cluster is used in the material by default).*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
		TArray<FWaveParameter> WaveClusters;

	/* Individual Gerstner wave settings.
	 * (leave blank to use the default offsets).*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
		TArray<FWaveSetParameters> WaveSetOffsetsOverride;

	UPROPERTY(BlueprintReadOnly)
		float NetworkTimeOffset;

	UFUNCTION(BlueprintCallable, meta = (HidePin = "World"))
		FVector GetWaveHeightValue(const FVector& location, const UWorld* World = nullptr, bool HeightOnly = true, bool TwoIterations = false);

	// Returns the wave height at a determined location.
	// Same as GetWaveHeightValue, but only returns the vertical component.
	float GetWaveHeight(const FVector& location, const UWorld* World = nullptr) const;

	// Landscape height modulation vars.
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
		bool bEnableLandscapeModulation;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
		float ModulationStartHeight = -1000.0f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
		float ModulationMaxHeight = 3200.0f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
		float ModulationPower = 4.0f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
		ALandscape* Landscape;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
		UTexture2D* HeightmapTexture;

	UFUNCTION(BlueprintCallable)
		void LoadLandscapeHeightmap(UTexture2D* Tex2D);

	UFUNCTION(BlueprintCallable)
		FLinearColor GetHeightmapPixel(float U, float V) const;




	// Used to enable / disable PP
	UPROPERTY(BlueprintReadOnly)
		bool bIsPPEnabled = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
		float PPDisableOffset = 100.0f;

	float GetTimeSeconds() const;

	FTimerHandle TimeResyncHandle;

	/* It's timer delay */
	UPROPERTY(EditDefaultsOnly)
		float TimeResyncTime = 0.5;

	// Used for time sync (Has server time.)
	UPROPERTY()
		mutable AGameStateBase* GameState;

	UPROPERTY(BlueprintAssignable)
		FOnQualityChanged OnQualityChanged;

	UPROPERTY(BlueprintAssignable)
		FOnUnderwaterPPChange OnUnderwaterPP_Enable;

	UPROPERTY(BlueprintAssignable)
		FOnUnderwaterPPChange OnUnderwaterPP_Disable;

protected:
	TArray<FFloat16Color> HeightmapPixels;
	int32 HeightmapWidth;
	int32 HeightmapHeight;

	mutable TArray<FWaveCache> WaveParameterCache;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
		UMaterialParameterCollection* MPC_Ocean;

	UPROPERTY()
		UMaterialParameterCollectionInstance* MPC_Ocean_Instance;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
		EOceanQuality CachedOceanQuality;

	/* Has to be called from BP. Recreates everything ... */
	UFUNCTION(BlueprintCallable, meta = (MaterialParameterCollectionFunction = "true"))
		void MaterialSetup();

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent)
		void UpdateQuality();
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent)
		void EnableCaustics();
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent)
		void DisableCaustics();

	/**
	 * Create wave set or wave sets.
	 * It should now support any ammount of wave sets.
	 * BUT will require proper MPC setup!
	 */
	void CreateWaveSet() const;

	/* Sets parameters to MPC. */
	void CreateDisplayParameters() const;

	void SetGlobalParameters();

	/* Default is very high. Should be overriden. */
	UFUNCTION(BlueprintNativeEvent)
		EOceanQuality GetOceanQuality();

	FORCEINLINE static FLinearColor MakeColorFromWave(const FWaveParameter Param);
	FORCEINLINE static FLinearColor MakeColorFromVector(const FVector Param);


	FORCEINLINE void SetScalarMPC(FName ParamName, float In) const;
	FORCEINLINE void SetVectorMPC(FName ParamName, FLinearColor In) const;

	// Based on the parameters of the wave sets, the time and the position, computes the wave height.
	// Same as CalculateGerstnerWaveSetVector, but only returns the vertical component.
	float CalculateGerstnerWaveSetHeight(const FVector& position, float time) const;

	FVector CalculateGerstnerWaveSetVector(const FVector& position, float time, bool CalculateXY, bool CalculateZ) const;
	FVector CalculateGerstnerWaveVector(float rotation, float waveLength, float amplitude, float steepness, const FVector2D& direction, const FVector& position, float time, FWaveCache& InWaveCache, bool CalculateXY, bool CalculateZ) const;

public:
	virtual void BeginPlay() override;
	virtual void EndPlay(EEndPlayReason::Type EndPlayReason) override;

	virtual void Tick(float DeltaSeconds) override;
#ifdef WITH_EDITORONLY_DATA
	virtual bool ShouldTickIfViewportsOnly() const override;
#endif

	UFUNCTION(BlueprintGetter)
		EOceanQuality GetCachedOceanQuality() const;

	/*
	 * Updates CPU and GPU time offsets.
	 * Starts in BeginPlay() and end in EndPlay()
	 * Uses timer
	 */
	UFUNCTION(BlueprintCallable)
		void CacheTimeOffset();

	/*
	 *  Returns time difference beetween server and client
	 *  Might be quite expensive. Use with caution.
	 *  You can use CacheTimeOffset() Once and get NetWorkTimeOffset Variable instead.
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure)
		float GetTimeOffset() const;

};
