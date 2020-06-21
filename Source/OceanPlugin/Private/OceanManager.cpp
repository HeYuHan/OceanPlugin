// For copyright see LICENSE in EnvironmentProject root dir, or:
//https://github.com/UE4-OceanProject/OceanProject/blob/Master-Environment-Project/LICENSE

#include "OceanManager.h"
#include "Engine/World.h"
#include "Engine/Texture2D.h"
#include "InfiniteSystemComponent.h"
#include "Components/PlanarReflectionComponent.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "Materials/MaterialParameterCollection.h"
#include "Materials/MaterialParameterCollectionInstance.h"
#include "GameFramework/GameState.h"
#include "Components/BillboardComponent.h"
#include "Kismet/GameplayStatics.h"

DEFINE_LOG_CATEGORY(LogOcean);

bool FWaveCache::GetDir(float rotation, const FVector2D& inDirection, FVector* outDir)
{
	if (rotation == LastRotation && inDirection == LastDirection)
	{
		*outDir = MemorizedDir;
		return true;
	}
	return false;
}

void FWaveCache::SetDir(float rotation, const FVector2D& inDirection, const FVector& inDir)
{
	LastDirection = inDirection;
	LastRotation = rotation;
	MemorizedDir = inDir;
}

AOceanManager::AOceanManager(const class FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = true;

	// Ocean parameters
	WaveClusters.AddDefaulted(1);
	WaveParameterCache.AddDefaulted(8);

	// Create Components
	IconComp = CreateDefaultSubobject<UBillboardComponent>(TEXT("Icon"));
	RootComponent = IconComp;

	OceanMeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("OceanMesh"));
	OceanMeshComp->AttachToComponent(IconComp, FAttachmentTransformRules::KeepRelativeTransform);
	OceanMeshComp->SetRelativeScale3D(FVector(4.7f, 4.7f, 1.0f));
	OceanMeshComp->SetGenerateOverlapEvents(false);
	OceanMeshComp->CastShadow = false;
	OceanMeshComp->IndirectLightingCacheQuality = EIndirectLightingCacheQuality::ILCQ_Volume;
	OceanMeshComp->bRenderInDepthPass = false;
	OceanMeshComp->bReceivesDecals = false;
	OceanMeshComp->TranslucencySortPriority = -1;

	OceanMeshUnderComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("OceanMeshUnderside"));
	OceanMeshUnderComp->AttachToComponent(OceanMeshComp, FAttachmentTransformRules::KeepRelativeTransform);
	OceanMeshUnderComp->SetGenerateOverlapEvents(false);
	OceanMeshUnderComp->CastShadow = false;
	OceanMeshUnderComp->bRenderInMainPass = false;
	OceanMeshUnderComp->bRenderInDepthPass = false;
	OceanMeshUnderComp->bReceivesDecals = false;
	OceanMeshUnderComp->bTreatAsBackgroundForOcclusion = true;
	OceanMeshUnderComp->bUseAsOccluder = true;
	OceanMeshUnderComp->bRenderCustomDepth = true;
	OceanMeshUnderComp->CustomDepthStencilValue = 1;
	OceanMeshUnderComp->TranslucencySortPriority = -1;

	OceanMeshTopComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("OceanMeshTopside"));
	OceanMeshTopComp->AttachToComponent(OceanMeshComp, FAttachmentTransformRules::KeepRelativeTransform);
	OceanMeshTopComp->SetGenerateOverlapEvents(false);
	OceanMeshTopComp->CastShadow = false;
	OceanMeshTopComp->bRenderInMainPass = false;
	OceanMeshTopComp->bRenderInDepthPass = false;
	OceanMeshTopComp->bReceivesDecals = false;
	OceanMeshTopComp->bTreatAsBackgroundForOcclusion = true;
	OceanMeshTopComp->bUseAsOccluder = true;
	OceanMeshTopComp->bRenderCustomDepth = true;
	OceanMeshTopComp->TranslucencySortPriority = -1;



	InfiniteSystemComp = CreateDefaultSubobject<UInfiniteSystemComponent>(TEXT("InfiniteSystem"));
	InfiniteSystemComp->AttachToComponent(OceanMeshComp, FAttachmentTransformRules::KeepRelativeTransform);
	InfiniteSystemComp->ScaleMin = 1.5f;
	InfiniteSystemComp->ScaleMax = 15.0f;

	PlanarReflectionComp = CreateDefaultSubobject<UPlanarReflectionComponent>(TEXT("PlanarReflection"));
	PlanarReflectionComp->AttachToComponent(OceanMeshComp, FAttachmentTransformRules::KeepRelativeTransform);
	PlanarReflectionComp->SetRelativeScale3D(FVector(1000.0f, 1000.0f, 1.0f));
	PlanarReflectionComp->NormalDistortionStrength = 400.0f;
	PlanarReflectionComp->PrefilterRoughness = 0.0f;
	PlanarReflectionComp->DistanceFromPlaneFadeoutStart = 400.0f;
	PlanarReflectionComp->DistanceFromPlaneFadeoutEnd = 800.0f;
	PlanarReflectionComp->AngleFromPlaneFadeStart = 0.0f;
	PlanarReflectionComp->AngleFromPlaneFadeEnd = 15.0f;
	// Disable preview plane (Needed UE 4.24+)
	PlanarReflectionComp->bShowPreviewPlane = false;
	PlanarReflectionComp->bRenderSceneTwoSided = true;
	PlanarReflectionComp->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_RenderScenePrimitives;


}

void AOceanManager::BeginPlay()
{
	Super::BeginPlay();

	FTimerDelegate ResyncDelegate;
	ResyncDelegate.BindUFunction(this, FName("CacheTimeOffset"));

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(TimeResyncHandle, ResyncDelegate, TimeResyncTime, true, 0.2);
	}

	//if (HeightmapTexture)
	//{
	//	LoadLandscapeHeightmap(HeightmapTexture);
	//}
}

void AOceanManager::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(TimeResyncHandle);
	}
}

void AOceanManager::Tick(const float DeltaSeconds)
{
	// Hide PP if above water.
	if (APlayerCameraManager* CameraManager = UGameplayStatics::GetPlayerCameraManager(this, 0))
	{
		const FVector Location = CameraManager->GetCameraLocation();

		// Should enable?
		const float WaterPoint = GetWaveHeightValue(Location).Z;
		const float TestPoint = Location.Z + PPDisableOffset;
		if (TestPoint > WaterPoint)
		{
			// Enable if not enabled:
			if (!bIsPPEnabled)
			{
				if (OnUnderwaterPP_Enable.IsBound()) OnUnderwaterPP_Enable.Broadcast();
			}
		}
		else
		{
			// Disable if enabled:
			if (bIsPPEnabled)
			{
				if (OnUnderwaterPP_Disable.IsBound()) OnUnderwaterPP_Disable.Broadcast();
			}
		}
	}
}

#ifdef WITH_EDITORONLY_DATA
bool AOceanManager::ShouldTickIfViewportsOnly() const
{
	return true;
}
#endif

void AOceanManager::MaterialSetup()
{
	if (MPC_Ocean)
	{
		if (UWorld* World = GetWorld())
		{
			if (World) MPC_Ocean_Instance = World->GetParameterCollectionInstance(MPC_Ocean);
		}
	}

	if (!IsValid(MID_Ocean) || !IsValid(MID_Ocean_Depth)) return;

	// Time offset (online)
	CacheTimeOffset();

	// Base setup
	CreateDisplayParameters();

	// Quality update
	UpdateQuality();

	// Wave set
	//CreateWaveSet();
	//SetGlobalParameters();

	// Reflections
	if (PlanarReflectionComp)
	{
		PlanarReflectionComp->HiddenActors = HiddenActors;
		//PlanarReflectionComp->HiddenActors.Add(this); Not sure if we want this. Disabled for now
	}
}

void AOceanManager::EnableCaustics_Implementation() {}

void AOceanManager::DisableCaustics_Implementation() {}

void AOceanManager::UpdateQuality_Implementation()
{
	if (!IsValid(PlanarReflectionComp)) return;

	CachedOceanQuality = GetOceanQuality();

	// Additional settings that might be used
	// (I handle it otherwise already, custom plugin ...)
	//PlanarReflectionComp->SetVisibility(false); 

	switch (CachedOceanQuality)
	{
	case EOceanQuality::Low:
		SetScalarMPC("TesselationMultiplier", 0.0f);
		OceanMeshComp->SetForcedLodModel(2);
		OceanMeshTopComp->SetForcedLodModel(2);
		DisableCaustics();
		break;
	case EOceanQuality::Medium:
		SetScalarMPC("TesselationMultiplier", 0.0f);
		OceanMeshComp->SetForcedLodModel(1);
		OceanMeshTopComp->SetForcedLodModel(1);
		DisableCaustics();
		break;
	case EOceanQuality::High:
		SetScalarMPC("TesselationMultiplier", 0.2f);
		OceanMeshComp->SetForcedLodModel(0);
		OceanMeshTopComp->SetForcedLodModel(0);
		PlanarReflectionComp->ScreenPercentage = 50.0f;
		DisableCaustics();
		break;
	case EOceanQuality::VeryHigh:
		SetScalarMPC("TesselationMultiplier", 0.8);
		OceanMeshComp->SetForcedLodModel(0);
		OceanMeshTopComp->SetForcedLodModel(0);
		PlanarReflectionComp->ScreenPercentage = 100.0f;
		EnableCaustics();
		break;
	case EOceanQuality::Cinematic:
		SetScalarMPC("TesselationMultiplier", 1.0f);
		OceanMeshComp->SetForcedLodModel(0);
		OceanMeshTopComp->SetForcedLodModel(0);
		PlanarReflectionComp->ScreenPercentage = 100.0f;
		EnableCaustics();
		break;
	}

	if (OnQualityChanged.IsBound())
		OnQualityChanged.Broadcast(CachedOceanQuality);
}

void AOceanManager::CreateWaveSet() const
{
	TArray<FWaveSetParameters> SetParams;

	if (WaveSetOffsetsOverride.Num() > 0)
	{
		// Set override
		SetParams = WaveSetOffsetsOverride;
	}
	else
	{
		// Add default
		SetParams.Add(FWaveSetParameters());
	}

	for (FWaveSetParameters& WaveSet : SetParams)
	{
		SetVectorMPC(WaveSet.Wave01.WaveName, MakeColorFromWave(WaveSet.Wave01));
		SetVectorMPC(WaveSet.Wave02.WaveName, MakeColorFromWave(WaveSet.Wave02));
		SetVectorMPC(WaveSet.Wave03.WaveName, MakeColorFromWave(WaveSet.Wave03));
		SetVectorMPC(WaveSet.Wave04.WaveName, MakeColorFromWave(WaveSet.Wave04));
		SetVectorMPC(WaveSet.Wave05.WaveName, MakeColorFromWave(WaveSet.Wave05));
		SetVectorMPC(WaveSet.Wave06.WaveName, MakeColorFromWave(WaveSet.Wave06));
		SetVectorMPC(WaveSet.Wave07.WaveName, MakeColorFromWave(WaveSet.Wave07));
		SetVectorMPC(WaveSet.Wave08.WaveName, MakeColorFromWave(WaveSet.Wave08));
	}
}

void AOceanManager::CreateDisplayParameters() const
{
	// 1.
	SetVectorMPC("BaseColorDark", BaseColorDark);
	SetVectorMPC("BaseColorLight", BaseColorLight);
	SetScalarMPC("BaseColorLerp", BaseColorLerp);
	SetScalarMPC("BaseFresnelPower", FresnelPower);
	SetScalarMPC("BaseFresnelReflect", BaseFresnelReflect);
	SetScalarMPC("Metallic", Metallic);
	SetScalarMPC("Roughness", Roughness);
	SetScalarMPC("Specular", Specular);
	SetScalarMPC("TesselationMultiplier", TesselationMultiplier);
	SetVectorMPC("ShallowColor", ShallowWaterColor);

	// 2.
	SetScalarMPC("Opacity", Opacity);
	SetScalarMPC("DepthFade", BaseDepthFade);
	SetScalarMPC("DistortionStrength", DistortionStrength);
	SetScalarMPC("SceneColorDepthFade", SceneColorCustomDepth);
	SetScalarMPC("FoamScale", FoamScale);
	SetScalarMPC("DepthTest1", FoamDepth1);
	SetScalarMPC("DepthTest2", FoamDepth2);
	SetScalarMPC("FoamTimeScale", FoamTimeScale);
	SetScalarMPC("FoamSoftness", FoamSoftness);
	SetScalarMPC("SceneDepthSoftness", SceneDepthSoftness);
	SetScalarMPC("BaseDepthFadeSoftness", BaseDepthFadeSoftness);

	// 3.
	SetVectorMPC("SSS_Color", SSS_Color);
	SetScalarMPC("SSS_Scale", SSS_Scale);
	SetScalarMPC("SSS_Intensity", SSS_Intensity);
	SetScalarMPC("SSS_LightDepth", SSS_LightDepth);
	SetScalarMPC("SSS_MacroNormalStrength", SSS_MacroNormalStrength);

	// 4.1 //OLD SHADER
	SetScalarMPC("PanWaveLerp", PanWaveLerp);
	SetScalarMPC("PanWaveIntensity", PanWaveIntensity);
	SetScalarMPC("PanWaveTimeScale", PanWaveTimeScale);
	SetScalarMPC("PanWaveSize", PanWaveSize);
	SetVectorMPC("PanWave01SpeedV2", MakeColorFromVector(Panner01Speed));
	SetVectorMPC("PanWave02SpeedV2", MakeColorFromVector(Panner02Speed));
	SetVectorMPC("PanWave03SpeedV2", MakeColorFromVector(Panner03Speed));

	// 4.2 //OLD SHADER
	SetScalarMPC("MacroScale", MacroWaveScale);
	SetScalarMPC("MacroSpeed", MacroWaveSpeed);
	SetScalarMPC("MacroAmplify", MacroWaveAmplify);

	// 5.
	SetScalarMPC("DetailNormalScale", DetailNormalScale);
	SetScalarMPC("DetailNormalSpeed", DetailNormalSpeed);
	SetScalarMPC("DetailNormalStrength", DetailNormalStrength);
	SetScalarMPC("MediumNormalScale", MediumNormalScale);
	SetScalarMPC("MediumNormalSpeed", MediumNormalSpeed);
	SetScalarMPC("MediumNormalStrength", MediumNormalStrength);
	SetScalarMPC("MediumNormalBlendDistance", MediumNormalBlendDistance);
	SetScalarMPC("MediumNormalFalloff", MediumNormalBlendFalloff);
	SetScalarMPC("FarNormalScale", FarNormalScale);
	SetScalarMPC("FarNormalSpeed", FarNormalSpeed);
	SetScalarMPC("FarNormalStrength", FarNormalStrength);
	SetScalarMPC("FarNormalBlendDistance", FarNormalBlendDistance);
	SetScalarMPC("FarNormalBlendFalloff", FarNormalBlendFalloff);

	// 6.
	SetScalarMPC("HeightmapDisplacement", HeightmapDisplacement);
	SetScalarMPC("HeightmapScale", HeightmapScale);
	SetScalarMPC("HeightmapSpeed", HeightmapSpeed);

	// 7.1
	SetScalarMPC("SeafoamScale", SeafoamScale);
	SetScalarMPC("SeafoamSpeed", SeafoamSpeed);
	SetScalarMPC("SeafoamDistortion", SeafoamDistortion);
	SetScalarMPC("SeafoamHeightPower", SeafoamHeightPower);
	SetScalarMPC("SeafoamHeightMultiply", SeafoamHeightMultiply);

	// 7.2
	SetScalarMPC("FoamCapsOpacity", FoamCapsOpacity);
	SetScalarMPC("FoamCapsHeight", FoamCapsHeight);
	SetScalarMPC("FoamCapsPower", FoamCapsPower);
	SetScalarMPC("FoamDistance", FoamPannerDistance);
	SetScalarMPC("FoamFalloff", FoamPannerFalloff);
	SetScalarMPC("CubemapReflectionStrength", CubemapReflectionStrength);

	// Set the base sea level (used by various materials)
	SetScalarMPC("SeaLevel", OceanMeshComp->GetComponentLocation().Z);

	// 8. Textures
	MID_Ocean->SetTextureParameterValue("Wave Normal", MediumWaveNormal);
	MID_Ocean->SetTextureParameterValue("Small Wave Normal", SmallWaveNormal);
	MID_Ocean->SetTextureParameterValue("Far Wave Normal", FarWaveNormal);
	MID_Ocean->SetTextureParameterValue("Large Wave Height", HeightmapWaves);
	MID_Ocean_Depth->SetTextureParameterValue("Large Wave Height", HeightmapWaves);
	MID_Ocean->SetTextureParameterValue("ShoreFoam", ShoreFoam);
	MID_Ocean->SetTextureParameterValue("ShoreFoam2", ShoreFoam2);
	MID_Ocean->SetTextureParameterValue("ShoreFoamRoughness", ShoreFoamRoughness);
	MID_Ocean->SetTextureParameterValue("Seafoam Texture", Seafoam);
	MID_Ocean->SetTextureParameterValue("Reflection Cubemap", ReflectionCubemap);
}

void AOceanManager::SetGlobalParameters()
{
	if (WaveClusters.Num() > 0)
	{
		FWaveParameter& Wave = WaveClusters[0];

		SetVectorMPC("WavesDirectionV2", FLinearColor(
			GlobalWaveDirection.X,
			GlobalWaveDirection.Y,
			0.0f,
			0.0f
		));

		SetScalarMPC("WaveSetRotation", Wave.Rotation);
		SetScalarMPC("WaveSetLength", Wave.Length);
		SetScalarMPC("WaveSetAmplitude", Wave.Amplitude * GlobalWaveAmplitude);
		SetScalarMPC("WaveSetSteepness", Wave.Steepness);
		SetScalarMPC("WaveSetTimeMultiplier", Wave.TimeScale * GlobalWaveSpeed);
	}

	if (IsValid(Landscape) && IsValid(HeightmapTexture))
	{
		SetVectorMPC("HeightmapSizeXY", FLinearColor(
			HeightmapTexture->GetSizeX(),
			HeightmapTexture->GetSizeY(),
			0.0f,
			0.0f
		));

		SetScalarMPC("ModulationStartHeight", ModulationStartHeight);
		SetScalarMPC("ModulationMaxHeight", ModulationMaxHeight);
		SetScalarMPC("ModulationPower", ModulationPower);

		SetVectorMPC("LandscapeLoc", FLinearColor(Landscape->GetActorLocation()));
		SetVectorMPC("LandscapeScale", FLinearColor(Landscape->GetActorScale()));

		MID_Ocean->SetTextureParameterValue("LandHeightmap", HeightmapTexture);
		MID_Ocean_Depth->SetTextureParameterValue("LandHeightmap", HeightmapTexture);
	}
}

EOceanQuality AOceanManager::GetOceanQuality_Implementation()
{
	return EOceanQuality::VeryHigh;
}

FLinearColor AOceanManager::MakeColorFromWave(const FWaveParameter Param)
{
	return FLinearColor(Param.Rotation, Param.Length, Param.Amplitude, Param.Steepness);
}

FLinearColor AOceanManager::MakeColorFromVector(const FVector Param)
{
	return FLinearColor(Param);
}

float AOceanManager::GetWaveHeight(const FVector& location, const UWorld* World) const
{
	// Flat ocean buoyancy optimization
	if (!EnableGerstnerWaves)
		return RootComponent->GetComponentLocation().Z;

	// GetWorld() can have a significant impact on the performace of this function, so let's give the caller the option to supply a cached result.
	const float time = GetTimeSeconds();

	//Landscape height modulation
	float LandscapeModulation = 1.0f;
	if (bEnableLandscapeModulation && IsValid(Landscape))
	{
		const FVector LandLoc = Landscape->GetActorLocation();
		const FVector2D LandXY = FVector2D(LandLoc.X, LandLoc.Y);
		const FVector2D LocXY = FVector2D(location.X, location.Y);
		const FVector LandScale = Landscape->GetActorScale3D();
		const FVector2D ScaleXY = FVector2D(LandScale.X * HeightmapWidth, LandScale.Y * HeightmapHeight);

		if (LocXY > LandXY&& LocXY < LandXY + ScaleXY) //optimization: don't calculate modulation if outside of landscape bounds
		{
			FVector2D UV = LocXY - (LandXY + ScaleXY / 2.0f);
			UV = UV / ScaleXY + 0.5f;

			float height = GetHeightmapPixel(UV.X, UV.Y).R - 0.5f;
			height = height * 512 * LandScale.Z + LandLoc.Z;

			LandscapeModulation = height - RootComponent->GetComponentLocation().Z - ModulationStartHeight;
			LandscapeModulation /= FMath::Abs(ModulationStartHeight - ModulationMaxHeight);
			LandscapeModulation = 1 - FMath::Clamp(LandscapeModulation, 0.0f, 1.0f);
			LandscapeModulation = FMath::Pow(LandscapeModulation, ModulationPower);
		}
	}

	// Calculate the Gerstner Wave Sets
	return CalculateGerstnerWaveSetHeight(location, time * GlobalWaveSpeed) * LandscapeModulation + RootComponent->GetComponentLocation().Z;
}

void AOceanManager::LoadLandscapeHeightmap(UTexture2D* Tex2D)
{
	if (!Tex2D)
	{
		return;
	}

	Tex2D->SRGB = true;
	//Tex2D->Filter = TF_Trilinear;// TF_Bilinear;
	Tex2D->CompressionSettings = TC_VectorDisplacementmap;
	Tex2D->UpdateResource();

	FTexture2DMipMap* MyMipMap = &Tex2D->PlatformData->Mips[0];
	HeightmapWidth = MyMipMap->SizeX;
	HeightmapHeight = MyMipMap->SizeY;

	HeightmapPixels.Empty();

	FColor* FormatedImageData = static_cast<FColor*>(Tex2D->PlatformData->Mips[0].BulkData.Lock(LOCK_READ_ONLY));

	// 	HeightmapPixels.SetNum(HeightmapWidth * HeightmapHeight);
	// 	uint8* ArrayData = (uint8 *)HeightmapPixels.GetData();
	// 	FMemory::Memcpy(ArrayData, FormatedImageData, GPixelFormats[Tex2D->GetPixelFormat()].BlockBytes * HeightmapWidth * HeightmapHeight);

	if (FormatedImageData)
	{
		for (int i = 0; i < HeightmapWidth * HeightmapHeight; i++)
		{
			HeightmapPixels.Add(FLinearColor(FormatedImageData[i]));
		}
	}

	Tex2D->PlatformData->Mips[0].BulkData.Unlock();

	// 	UE_LOG(LogTemp, Warning, TEXT("num = %d"), HeightmapPixels.Num());
	// 	UE_LOG(LogTemp, Warning, TEXT("numx = %f"), (float)HeightmapPixels[0].R);
}

FLinearColor AOceanManager::GetHeightmapPixel(float U, float V) const
{
	if (HeightmapPixels.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("Landscape heightmap data is not loaded! Pixel array is empty."));
		return FLinearColor::Black;
	}

	const int32 Width = HeightmapWidth;
	const int32 Height = HeightmapHeight;

	check(Width > 0 && Height > 0 && HeightmapPixels.Num() > 0);

	const float NormalizedU = U >= 0 ? FMath::Fractional(U) : 1.0 + FMath::Fractional(U);
	const float NormalizedV = V >= 0 ? FMath::Fractional(V) : 1.0 + FMath::Fractional(V);

	const int PixelX = NormalizedU * (Width - 1) + 1;
	const int PixelY = NormalizedV * (Height - 1) + 1;

	return FLinearColor(HeightmapPixels[(PixelY - 1) * Width + PixelX - 1]);
}

FVector AOceanManager::GetWaveHeightValue(const FVector& location, const UWorld* World, bool HeightOnly, bool TwoIterations)
{
	const float SeaLevel = RootComponent->GetComponentLocation().Z;

	//optimization: skip gerstner calculations if gerstner waves are disabled or test point is far below/above from sea level
	if (!EnableGerstnerWaves || location.Z - DistanceCheck > SeaLevel || location.Z + DistanceCheck < SeaLevel)
		return FVector(0.0f, 0.0f, SeaLevel);

	const float time = GetTimeSeconds();

	//Landscape height modulation
	float LandscapeModulation = 1.0f;
	if (bEnableLandscapeModulation && IsValid(Landscape))
	{
		const FVector LandLoc = Landscape->GetActorLocation();
		const FVector2D LandXY = FVector2D(LandLoc.X, LandLoc.Y);
		const FVector2D LocXY = FVector2D(location.X, location.Y);
		const FVector LandScale = Landscape->GetActorScale3D();
		const FVector2D ScaleXY = FVector2D(LandScale.X * HeightmapWidth, LandScale.Y * HeightmapHeight);

		if (LocXY > LandXY&& LocXY < LandXY + ScaleXY) //optimization: don't calculate modulation if outside of landscape bounds
		{
			FVector2D UV = LocXY - (LandXY + ScaleXY / 2.0f);
			UV = UV / ScaleXY + 0.5f;

			float height = GetHeightmapPixel(UV.X, UV.Y).R - 0.5f;
			height = height * 512 * LandScale.Z + LandLoc.Z;

			LandscapeModulation = height - SeaLevel - ModulationStartHeight;
			LandscapeModulation /= FMath::Abs(ModulationStartHeight - ModulationMaxHeight);
			LandscapeModulation = 1 - FMath::Clamp(LandscapeModulation, 0.0f, 1.0f);
			LandscapeModulation = FMath::Pow(LandscapeModulation, ModulationPower);

			//DrawDebugPoint(World, FVector(location.X, location.Y, height), 15.0f, FColor::Blue);

// 			//Trace method (too slow)
// 			FHitResult hit(ForceInit);
// 			FVector rayStart = location + FVector(0, 0, 2000);
// 			FVector rayEnd = location - FVector(0, 0, 2000);
// 			FCollisionQueryParams params = FCollisionQueryParams(FName(TEXT("trace")), true, this);
// 			if (Landscape->ActorLineTraceSingle(hit, rayStart, rayEnd, ECollisionChannel::ECC_Visibility, params))
// 			{
// 				LandscapeModulation = hit.Location.Z - RootComponent->GetComponentLocation().Z - ModulationStartHeight;
// 				LandscapeModulation /= FMath::Abs(ModulationStartHeight - ModulationMaxHeight);
// 				LandscapeModulation = 1 - FMath::Clamp(LandscapeModulation, 0.0f, 1.0f);
// 				LandscapeModulation = FMath::Pow(LandscapeModulation, ModulationPower);
// 				
// 				DrawDebugPoint(World, hit.Location, 15.0f, FColor::Blue);
// 			}

		}
	}

	// Calculate the Gerstner Wave Sets
	if (TwoIterations)
	{
		const FVector xy = CalculateGerstnerWaveSetVector(location, time * GlobalWaveSpeed, true, false);
		const float z = CalculateGerstnerWaveSetVector(location - xy, time * GlobalWaveSpeed, false, true).Z;
		return FVector(xy.X * LandscapeModulation, xy.Y * LandscapeModulation, z * LandscapeModulation + SeaLevel);
	}
	return CalculateGerstnerWaveSetVector(location, time * GlobalWaveSpeed, !HeightOnly, true) * LandscapeModulation + FVector(0, 0, SeaLevel);
}

float AOceanManager::GetTimeSeconds() const
{
	if (UWorld* World = GetWorld())
	{
		return World->GetTimeSeconds() + NetworkTimeOffset;
	}

	return 0.0f;
}

void AOceanManager::SetScalarMPC(const FName ParamName, const float In) const
{
	if (MPC_Ocean_Instance)
	{
		if (!MPC_Ocean_Instance->SetScalarParameterValue(ParamName, In))
		{
#ifdef UE_BUILD_DEBUG
			UE_LOG(LogOcean, Error, TEXT("SetScalarMPC(): MPC Param: %s not found."), *ParamName.ToString());
#endif
		}
	}
}

void AOceanManager::SetVectorMPC(const FName ParamName, const FLinearColor In) const
{
	if (MPC_Ocean_Instance)
	{
		if (!MPC_Ocean_Instance->SetVectorParameterValue(ParamName, In))
		{
#ifdef UE_BUILD_DEBUG
			UE_LOG(LogOcean, Error, TEXT("SetVectorMPC(): MPC Param: %s not found."), *ParamName.ToString());
#endif
		}
	}
}

float AOceanManager::CalculateGerstnerWaveSetHeight(const FVector& position, float time) const
{
	return CalculateGerstnerWaveSetVector(position, time, false, true).Z;

	//	float sum = 0.0f;
	// 	// Calculate the Gerstner Waves
	// 	sum += CalculateGerstnerWaveHeight(global.Rotation + ws.Wave01.Rotation, global.Length * ws.Wave01.Length,
	// 		global.Amplitude * ws.Wave01.Amplitude, global.Steepness * ws.Wave01.Steepness, direction, position, time, WaveParameterCache[0]);
	// 	sum += CalculateGerstnerWaveHeight(global.Rotation + ws.Wave02.Rotation, global.Length * ws.Wave02.Length,
	// 		global.Amplitude * ws.Wave02.Amplitude, global.Steepness * ws.Wave02.Steepness, direction, position, time, WaveParameterCache[1]);
	// 	sum += CalculateGerstnerWaveHeight(global.Rotation + ws.Wave03.Rotation, global.Length * ws.Wave03.Length,
	// 		global.Amplitude * ws.Wave03.Amplitude, global.Steepness * ws.Wave03.Steepness, direction, position, time, WaveParameterCache[2]);
	// 	sum += CalculateGerstnerWaveHeight(global.Rotation + ws.Wave04.Rotation, global.Length * ws.Wave04.Length,
	// 		global.Amplitude * ws.Wave04.Amplitude, global.Steepness * ws.Wave04.Steepness, direction, position, time, WaveParameterCache[3]);
	// 	sum += CalculateGerstnerWaveHeight(global.Rotation + ws.Wave05.Rotation, global.Length * ws.Wave05.Length,
	// 		global.Amplitude * ws.Wave05.Amplitude, global.Steepness * ws.Wave05.Steepness, direction, position, time, WaveParameterCache[4]);
	// 	sum += CalculateGerstnerWaveHeight(global.Rotation + ws.Wave06.Rotation, global.Length * ws.Wave06.Length,
	// 		global.Amplitude * ws.Wave06.Amplitude, global.Steepness * ws.Wave06.Steepness, direction, position, time, WaveParameterCache[5]);
	// 	sum += CalculateGerstnerWaveHeight(global.Rotation + ws.Wave07.Rotation, global.Length * ws.Wave07.Length,
	// 		global.Amplitude * ws.Wave07.Amplitude, global.Steepness * ws.Wave07.Steepness, direction, position, time, WaveParameterCache[6]);
	// 	sum += CalculateGerstnerWaveHeight(global.Rotation + ws.Wave08.Rotation, global.Length * ws.Wave08.Length,
	// 		global.Amplitude * ws.Wave08.Amplitude, global.Steepness * ws.Wave08.Steepness, direction, position, time, WaveParameterCache[7]);
	// 
	// 	return sum / 8.0f;
}

FVector AOceanManager::CalculateGerstnerWaveSetVector(const FVector& position, float time, bool CalculateXY, bool CalculateZ) const
{
	FVector sum = FVector(0, 0, 0);

	if (WaveClusters.Num() <= 0)
		return sum;

	for (int i = 0; i < WaveClusters.Num(); i++)
	{
		FWaveSetParameters offsets = FWaveSetParameters();
		if (WaveSetOffsetsOverride.IsValidIndex(i))
		{
			offsets = WaveSetOffsetsOverride[i];
		}

		sum += CalculateGerstnerWaveVector(WaveClusters[i].Rotation + offsets.Wave01.Rotation, WaveClusters[i].Length * offsets.Wave01.Length,
			GlobalWaveAmplitude * WaveClusters[i].Amplitude * offsets.Wave01.Amplitude, WaveClusters[i].Steepness * offsets.Wave01.Steepness, GlobalWaveDirection,
			position, WaveClusters[i].TimeScale * offsets.Wave01.TimeScale * time, WaveParameterCache[0], CalculateXY, CalculateZ);

		sum += CalculateGerstnerWaveVector(WaveClusters[i].Rotation + offsets.Wave02.Rotation, WaveClusters[i].Length * offsets.Wave02.Length,
			GlobalWaveAmplitude * WaveClusters[i].Amplitude * offsets.Wave02.Amplitude, WaveClusters[i].Steepness * offsets.Wave02.Steepness, GlobalWaveDirection,
			position, WaveClusters[i].TimeScale * offsets.Wave02.TimeScale * time, WaveParameterCache[1], CalculateXY, CalculateZ);

		sum += CalculateGerstnerWaveVector(WaveClusters[i].Rotation + offsets.Wave03.Rotation, WaveClusters[i].Length * offsets.Wave03.Length,
			GlobalWaveAmplitude * WaveClusters[i].Amplitude * offsets.Wave03.Amplitude, WaveClusters[i].Steepness * offsets.Wave03.Steepness, GlobalWaveDirection,
			position, WaveClusters[i].TimeScale * offsets.Wave03.TimeScale * time, WaveParameterCache[2], CalculateXY, CalculateZ);

		sum += CalculateGerstnerWaveVector(WaveClusters[i].Rotation + offsets.Wave04.Rotation, WaveClusters[i].Length * offsets.Wave04.Length,
			GlobalWaveAmplitude * WaveClusters[i].Amplitude * offsets.Wave04.Amplitude, WaveClusters[i].Steepness * offsets.Wave04.Steepness, GlobalWaveDirection,
			position, WaveClusters[i].TimeScale * offsets.Wave04.TimeScale * time, WaveParameterCache[3], CalculateXY, CalculateZ);

		sum += CalculateGerstnerWaveVector(WaveClusters[i].Rotation + offsets.Wave05.Rotation, WaveClusters[i].Length * offsets.Wave05.Length,
			GlobalWaveAmplitude * WaveClusters[i].Amplitude * offsets.Wave05.Amplitude, WaveClusters[i].Steepness * offsets.Wave05.Steepness, GlobalWaveDirection,
			position, WaveClusters[i].TimeScale * offsets.Wave05.TimeScale * time, WaveParameterCache[4], CalculateXY, CalculateZ);

		sum += CalculateGerstnerWaveVector(WaveClusters[i].Rotation + offsets.Wave06.Rotation, WaveClusters[i].Length * offsets.Wave06.Length,
			GlobalWaveAmplitude * WaveClusters[i].Amplitude * offsets.Wave06.Amplitude, WaveClusters[i].Steepness * offsets.Wave06.Steepness, GlobalWaveDirection,
			position, WaveClusters[i].TimeScale * offsets.Wave06.TimeScale * time, WaveParameterCache[5], CalculateXY, CalculateZ);

		sum += CalculateGerstnerWaveVector(WaveClusters[i].Rotation + offsets.Wave07.Rotation, WaveClusters[i].Length * offsets.Wave07.Length,
			GlobalWaveAmplitude * WaveClusters[i].Amplitude * offsets.Wave07.Amplitude, WaveClusters[i].Steepness * offsets.Wave07.Steepness, GlobalWaveDirection,
			position, WaveClusters[i].TimeScale * offsets.Wave07.TimeScale * time, WaveParameterCache[6], CalculateXY, CalculateZ);

		sum += CalculateGerstnerWaveVector(WaveClusters[i].Rotation + offsets.Wave08.Rotation, WaveClusters[i].Length * offsets.Wave08.Length,
			GlobalWaveAmplitude * WaveClusters[i].Amplitude * offsets.Wave08.Amplitude, WaveClusters[i].Steepness * offsets.Wave08.Steepness, GlobalWaveDirection,
			position, WaveClusters[i].TimeScale * offsets.Wave08.TimeScale * time, WaveParameterCache[7], CalculateXY, CalculateZ);
	}

	// 	// Calculate the Gerstner Waves
	// 	sum += CalculateGerstnerWaveVector(global.Rotation + ws.Wave01.Rotation, global.Length * ws.Wave01.Length,
	// 		global.Amplitude * ws.Wave01.Amplitude, global.Steepness * ws.Wave01.Steepness, direction, position, time, WaveParameterCache[0], CalculateXY, CalculateZ);
	// 	sum += CalculateGerstnerWaveVector(global.Rotation + ws.Wave02.Rotation, global.Length * ws.Wave02.Length,
	// 		global.Amplitude * ws.Wave02.Amplitude, global.Steepness * ws.Wave02.Steepness, direction, position, time, WaveParameterCache[1], CalculateXY, CalculateZ);
	// 	sum += CalculateGerstnerWaveVector(global.Rotation + ws.Wave03.Rotation, global.Length * ws.Wave03.Length,
	// 		global.Amplitude * ws.Wave03.Amplitude, global.Steepness * ws.Wave03.Steepness, direction, position, time, WaveParameterCache[2], CalculateXY, CalculateZ);
	// 	sum += CalculateGerstnerWaveVector(global.Rotation + ws.Wave04.Rotation, global.Length * ws.Wave04.Length,
	// 		global.Amplitude * ws.Wave04.Amplitude, global.Steepness * ws.Wave04.Steepness, direction, position, time, WaveParameterCache[3], CalculateXY, CalculateZ);
	// 	sum += CalculateGerstnerWaveVector(global.Rotation + ws.Wave05.Rotation, global.Length * ws.Wave05.Length,
	// 		global.Amplitude * ws.Wave05.Amplitude, global.Steepness * ws.Wave05.Steepness, direction, position, time, WaveParameterCache[4], CalculateXY, CalculateZ);
	// 	sum += CalculateGerstnerWaveVector(global.Rotation + ws.Wave06.Rotation, global.Length * ws.Wave06.Length,
	// 		global.Amplitude * ws.Wave06.Amplitude, global.Steepness * ws.Wave06.Steepness, direction, position, time, WaveParameterCache[5], CalculateXY, CalculateZ);
	// 	sum += CalculateGerstnerWaveVector(global.Rotation + ws.Wave07.Rotation, global.Length * ws.Wave07.Length,
	// 		global.Amplitude * ws.Wave07.Amplitude, global.Steepness * ws.Wave07.Steepness, direction, position, time, WaveParameterCache[6], CalculateXY, CalculateZ);
	// 	sum += CalculateGerstnerWaveVector(global.Rotation + ws.Wave08.Rotation, global.Length * ws.Wave08.Length,
	// 		global.Amplitude * ws.Wave08.Amplitude, global.Steepness * ws.Wave08.Steepness, direction, position, time, WaveParameterCache[7], CalculateXY, CalculateZ);

	return sum / (WaveClusters.Num() * 8);
}

FVector AOceanManager::CalculateGerstnerWaveVector(float rotation, float waveLength, float amplitude, float steepness, const FVector2D& direction, const FVector& position, float time, FWaveCache& InWaveCache, bool CalculateXY, bool CalculateZ) const
{
	float frequency = (2 * PI) / waveLength;

	FVector dir;
	if (!InWaveCache.GetDir(rotation, direction, &dir))
	{
		dir = FVector(direction.X, direction.Y, 0);
		dir = dir.RotateAngleAxis(rotation * 360, FVector(0, 0, 1));
		dir = dir.GetSafeNormal();
		InWaveCache.SetDir(rotation, direction, dir);
	}

	float wavePhase = frequency * FVector::DotProduct(dir, position) + time;
	float c = 0, s = 0, QA = 0;

	//FMath::SinCos(&s, &c, wavePhase);

	if (CalculateXY)
	{
		c = FMath::Cos(wavePhase);
		QA = steepness * amplitude;
	}

	if (CalculateZ)
	{
		s = FMath::Sin(wavePhase);
	}

	return FVector(QA * dir.X * c, QA * dir.Y * c, amplitude * s);
}

EOceanQuality AOceanManager::GetCachedOceanQuality() const
{
	return CachedOceanQuality;
}

void AOceanManager::CacheTimeOffset()
{
	// CPU Offset
	NetworkTimeOffset = GetTimeOffset();

	// GPU Offset
	SetScalarMPC("NetworkTimeOffset", NetworkTimeOffset);
}

//Compares the "world" time of client and server, because clients joining have "younger" worlds.
float AOceanManager::GetTimeOffset() const
{
	// If no world, auto-detect
	if (UWorld* World = GetWorld())
	{
		// If no GameState, auto-detect
		if (!GameState)
		{
			for (TActorIterator<AGameStateBase> ActorItr(GetWorld()); ActorItr; ++ActorItr)
			{
				GameState = Cast<AGameStateBase>(*ActorItr);
				break;
			}
		}

		if (!IsValid(GameState) || !IsValid(World)) return 0.0f;

		const float ServerTime = GameState->GetServerWorldTimeSeconds();
		const float LocalTime = World->GetTimeSeconds();

		return ServerTime - LocalTime;
	}

	return 0.0f;
}