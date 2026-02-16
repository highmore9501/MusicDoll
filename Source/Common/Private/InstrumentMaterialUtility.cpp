#include "InstrumentMaterialUtility.h"

#include "Materials/MaterialInstance.h"
#include "Materials/MaterialInstanceConstant.h"
#include "UObject/Package.h"

UMaterialInstanceConstant* UInstrumentMaterialUtility::CreateOrGetMaterialInstance(
    const FString& MaterialName,
    const FString& PackagePath,
    UMaterialInterface* ParentMaterial,
    TMap<FString, UMaterialInstanceConstant*>& CachedMaterials)
{
    if (MaterialName.IsEmpty() || PackagePath.IsEmpty())
    {
        UE_LOG(LogTemp, Error, TEXT("[InstrumentMaterialUtility] MaterialName or PackagePath is empty"));
        return nullptr;
    }

    if (!ParentMaterial)
    {
        UE_LOG(LogTemp, Error, TEXT("[InstrumentMaterialUtility] ParentMaterial is null for '%s'"), *MaterialName);
        return nullptr;
    }

    // 1. 首先检查缓存
    UMaterialInstanceConstant** CachedMaterial = CachedMaterials.Find(MaterialName);
    if (CachedMaterial && *CachedMaterial)
    {
        return *CachedMaterial;
    }

    // 2. 尝试从磁盘加载已存在的材质
    UMaterialInterface* ExistingMaterial = LoadObject<UMaterialInterface>(nullptr, *PackagePath);
    if (ExistingMaterial)
    {
        if (UMaterialInstanceConstant* MaterialInstance = Cast<UMaterialInstanceConstant>(ExistingMaterial))
        {
            // 缓存并返回
            CachedMaterials.Add(MaterialName, MaterialInstance);
            return MaterialInstance;
        }
    }

    // 3. 创建新的材质实例
    UPackage* Package = CreatePackage(*PackagePath);
    if (!Package)
    {
        UE_LOG(LogTemp, Error, TEXT("[InstrumentMaterialUtility] Failed to create package: %s"), *PackagePath);
        return nullptr;
    }

    UMaterialInstanceConstant* NewMaterialInstance = NewObject<UMaterialInstanceConstant>(
        Package,
        *MaterialName,
        RF_Public | RF_Standalone);

    if (!NewMaterialInstance)
    {
        UE_LOG(LogTemp, Error, TEXT("[InstrumentMaterialUtility] Failed to create material instance: %s"), *MaterialName);
        return nullptr;
    }

    // 设置父材质
    NewMaterialInstance->SetParentEditorOnly(ParentMaterial);

    // 标记包为脏状态
    Package->MarkPackageDirty();

    // 缓存并返回
    CachedMaterials.Add(MaterialName, NewMaterialInstance);

    UE_LOG(LogTemp, Log, TEXT("[InstrumentMaterialUtility] Created new material instance: %s"), *MaterialName);

    return NewMaterialInstance;
}

bool UInstrumentMaterialUtility::MaterialHasParameter(
    UMaterialInterface* Material,
    const FString& ParameterName)
{
    if (!Material)
    {
        return false;
    }

    FName ParameterFName(*ParameterName);

    // 尝试转换为材质实例
    if (UMaterialInstance* MaterialInstance = Cast<UMaterialInstance>(Material))
    {
        TArray<FMaterialParameterInfo> ParameterInfo;
        TArray<FGuid> ParameterIds;
        MaterialInstance->GetAllScalarParameterInfo(ParameterInfo, ParameterIds);

        for (const FMaterialParameterInfo& Info : ParameterInfo)
        {
            if (Info.Name == ParameterFName)
            {
                return true;
            }
        }
    }

    // 尝试转换为材质实例常量
    if (UMaterialInstanceConstant* MaterialInstanceConstant = Cast<UMaterialInstanceConstant>(Material))
    {
        TArray<FMaterialParameterInfo> ParameterInfo;
        TArray<FGuid> ParameterIds;
        MaterialInstanceConstant->GetAllScalarParameterInfo(ParameterInfo, ParameterIds);

        for (const FMaterialParameterInfo& Info : ParameterInfo)
        {
            if (Info.Name == ParameterFName)
            {
                return true;
            }
        }
    }

    return false;
}

int32 UInstrumentMaterialUtility::UpdateSkeletalMeshMaterials(
    USkeletalMeshComponent* SkeletalMeshComp,
    const FMaterialUpdateSettings& Settings,
    TMap<FString, UMaterialInstanceConstant*>& OutCachedMaterials)
{
    if (!SkeletalMeshComp)
    {
        UE_LOG(LogTemp, Error, TEXT("[InstrumentMaterialUtility] SkeletalMeshComp is null"));
        return 0;
    }

    if (!Settings.MaterialSelector)
    {
        UE_LOG(LogTemp, Error, TEXT("[InstrumentMaterialUtility] MaterialSelector is not set"));
        return 0;
    }

    int32 SuccessCount = 0;
    int32 FailureCount = 0;

    int32 NumMaterials = SkeletalMeshComp->GetNumMaterials();
    const TArray<FName>& MaterialSlotNames = SkeletalMeshComp->GetMaterialSlotNames();

    if (NumMaterials == 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("[InstrumentMaterialUtility] SkeletalMeshComp has no materials"));
        return 0;
    }

    UE_LOG(LogTemp, Log, TEXT("[InstrumentMaterialUtility] Processing %d materials..."), NumMaterials);

    // 遍历所有材质槽
    for (int32 MaterialSlotIndex = 0; MaterialSlotIndex < NumMaterials; ++MaterialSlotIndex)
    {
        UMaterialInterface* CurrentMaterial = SkeletalMeshComp->GetMaterial(MaterialSlotIndex);

        if (!CurrentMaterial)
        {
            continue;
        }

        FString MaterialName = CurrentMaterial->GetName();
        FString MaterialSlotName = MaterialSlotNames.IsValidIndex(MaterialSlotIndex)
                                       ? MaterialSlotNames[MaterialSlotIndex].ToString()
                                       : FString::Printf(TEXT("Slot_%d"), MaterialSlotIndex);

        // 检查是否跳过已动画化的材质
        if (Settings.bSkipAnimatedMaterials && MaterialName.Contains(TEXT("Animated")))
        {
            UE_LOG(LogTemp, Verbose, TEXT("[InstrumentMaterialUtility] Skipping animated material: %s"), *MaterialName);
            SuccessCount++;
            continue;
        }

        // 调用材质选择器函数
        UMaterialInterface* SelectedMaterial = Settings.MaterialSelector(MaterialSlotName, MaterialSlotIndex);

        if (!SelectedMaterial)
        {
            UE_LOG(LogTemp, Warning, TEXT("[InstrumentMaterialUtility] MaterialSelector returned null for slot '%s' (index %d)"),
                   *MaterialSlotName, MaterialSlotIndex);
            FailureCount++;
            continue;
        }

        // 应用材质
        SkeletalMeshComp->SetMaterial(MaterialSlotIndex, SelectedMaterial);
        SuccessCount++;
    }

    // 标记组件为脏状态
    if (SuccessCount > 0)
    {
        SkeletalMeshComp->MarkPackageDirty();
    }

    UE_LOG(LogTemp, Log,
           TEXT("[InstrumentMaterialUtility] Update complete: %d succeeded, %d failed"),
           SuccessCount, FailureCount);

    return SuccessCount;
}
