#pragma once

#include "CoreMinimal.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Materials/MaterialInterface.h"
#include "Components/SkeletalMeshComponent.h"
#include "UObject/NoExportTypes.h"
#include "InstrumentMaterialUtility.generated.h"

// 前置声明
class UMaterialInstanceConstant;
class UMaterialInterface;
class USkeletalMeshComponent;

/**
 * 材质更新设置
 * 用于配置批量材质更新的行为
 */
USTRUCT(BlueprintType)
struct COMMON_API FMaterialUpdateSettings
{
    GENERATED_BODY()

    /**
     * 材质选择器函数（根据槽名称和索引返回材质）
     * @param SlotName 材质槽名称
     * @param SlotIndex 材质槽索引
     * @return 要应用的材质实例
     */
    TFunction<UMaterialInterface*(const FString& SlotName, int32 SlotIndex)> MaterialSelector;

    /** 是否跳过已动画化的材质（名称包含"Animated"） */
    UPROPERTY()
    bool bSkipAnimatedMaterials = true;

    /** 材质名称模式（如 "MAT_{Type}_{Index}"） */
    UPROPERTY()
    FString MaterialNamePattern;

    /** 包路径模式（如 "/Game/Materials/{MaterialName}"） */
    UPROPERTY()
    FString PackagePathPattern = TEXT("/Game/Materials/{MaterialName}");

    FMaterialUpdateSettings()
        : bSkipAnimatedMaterials(true)
        , MaterialNamePattern(TEXT(""))
        , PackagePathPattern(TEXT("/Game/Materials/{MaterialName}"))
    {
    }
};

/**
 * 乐器材质工具类
 * 提供材质实例创建、缓存管理、参数检查等通用操作
 * 
 * 主要功能:
 * - 创建和缓存材质实例，避免重复创建
 * - 检查材质是否包含特定参数
 * - 批量更新骨骼网格的材质
 * 
 * 使用示例:
 * @code
 * // 检查材质是否有参数
 * if (UInstrumentMaterialUtility::MaterialHasParameter(Material, TEXT("Pressed"))) {
 *     // 材质有Pressed参数
 * }
 * 
 * // 创建或获取材质实例
 * UMaterialInstanceConstant* MatInstance = UInstrumentMaterialUtility::CreateOrGetMaterialInstance(
 *     TEXT("MAT_Key_75"),
 *     TEXT("/Game/Materials/MAT_Key_75"),
 *     ParentMaterial,
 *     CachedMaterials
 * );
 * @endcode
 */
UCLASS()
class COMMON_API UInstrumentMaterialUtility : public UObject
{
    GENERATED_BODY()

public:
    // ========== 材质实例管理 ==========

    /**
     * 创建或获取材质实例（带缓存）
     * 
     * 优先从缓存Map中查找，如果不存在则尝试从磁盘加载，
     * 如果仍不存在则创建新的材质实例并缓存。
     * 
     * @param MaterialName 材质名称（如 "MAT_Key_75"）
     * @param PackagePath 包路径（如 "/Game/Materials/MAT_Key_75"）
     * @param ParentMaterial 父材质
     * @param CachedMaterials 缓存Map（用于避免重复创建），使用材质名称作为Key
     * @return 创建或获取的材质实例，失败返回nullptr
     * 
     * @note 此方法会自动标记包为脏状态(MarkPackageDirty)
     * @note 缓存Map的Key通常使用简化的索引字符串（如钢琴键号"75"）
     */
    static UMaterialInstanceConstant* CreateOrGetMaterialInstance(
        const FString& MaterialName,
        const FString& PackagePath,
        UMaterialInterface* ParentMaterial,
        TMap<FString, UMaterialInstanceConstant*>& CachedMaterials);

    /**
     * 检查材质是否包含指定参数
     * 
     * 支持检查UMaterialInstance和UMaterialInstanceConstant中的标量参数
     * 
     * @param Material 要检查的材质
     * @param ParameterName 参数名称（如 "Pressed", "Vibration"）
     * @return 是否包含该参数
     * 
     * @note 仅检查标量参数(Scalar Parameter)
     */
    static bool MaterialHasParameter(
        UMaterialInterface* Material,
        const FString& ParameterName);

    /**
     * 批量更新骨骼网格的材质
     * 
     * 遍历骨骼网格的所有材质槽，根据Settings中的MaterialSelector函数
     * 为每个槽选择合适的材质并应用。
     * 
     * @param SkeletalMeshComp 骨骼网格组件
     * @param Settings 更新设置，包含材质选择器函数和其他配置
     * @param OutCachedMaterials 输出：缓存的材质实例Map
     * @return 成功更新的材质数量
     * 
     * @note MaterialSelector函数返回nullptr时会跳过该槽
     * @note 如果Settings.bSkipAnimatedMaterials为true，会跳过名称包含"Animated"的材质
     * 
     * @warning 确保在调用前SkeletalMeshComp已正确初始化
     */
    static int32 UpdateSkeletalMeshMaterials(
        USkeletalMeshComponent* SkeletalMeshComp,
        const FMaterialUpdateSettings& Settings,
        TMap<FString, UMaterialInstanceConstant*>& OutCachedMaterials);
};
