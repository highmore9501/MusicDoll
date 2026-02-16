#pragma once

#include "Animation/SkeletalMeshActor.h"
#include "ControlRig/Public/ControlRig.h"
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "InstrumentBase.h"
#include "Tickable.h"
#include "StringFlowUnreal.generated.h"

// 手部枚举
UENUM(BlueprintType)
enum class EStringFlowHandType : uint8 { LEFT = 0, RIGHT = 1 };

// 左手位置类型枚举
UENUM(BlueprintType)
enum class EStringFlowLeftHandPositionType : uint8 {
    NORMAL = 0,
    INNER = 1,
    OUTER = 2
};

// 右手位置类型枚举
UENUM(BlueprintType)
enum class EStringFlowRightHandPositionType : uint8 {
    NEAR = 0,
    FAR = 1,
    PIZZICATO = 2
};

// 左手品格索引枚举（品位选择）
UENUM(BlueprintType)
enum class EStringFlowLeftHandFretIndex : uint8 {
    FRET_1 = 0,
    FRET_9 = 1,
    FRET_12 = 2
};

// 右手弦索引枚举（弦选择）
UENUM(BlueprintType)
enum class EStringFlowRightHandStringIndex : uint8 {
    STRING_0 = 0,
    STRING_1 = 1,
    STRING_2 = 2,
    STRING_3 = 3
};

// 辅助结构体：字符串数组
USTRUCT(BlueprintType)
struct FStringFlowStringArray {
    GENERATED_BODY()

   public:
    /** 字符串数组 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Data")
    TArray<FString> Strings;

    FStringFlowStringArray() {}

    void Add(const FString& String) { Strings.Add(String); }

    int32 Num() const { return Strings.Num(); }

    FString Get(int32 Index) const {
        if (Index >= 0 && Index < Strings.Num()) {
            return Strings[Index];
        }
        return FString();
    }

    void Clear() { Strings.Empty(); }
};

// 记录器变换结构体
USTRUCT(BlueprintType)
struct FStringFlowRecorderTransform {
    GENERATED_BODY()

   public:
    /** 位置 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Transform")
    FVector Location;

    /** 旋转 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Transform")
    FQuat Rotation;

    FStringFlowRecorderTransform() : Location(ForceInit), Rotation(ForceInit) {}

    FStringFlowRecorderTransform(const FVector& InLocation,
                                 const FQuat& InRotation)
        : Location(InLocation), Rotation(InRotation) {}

    FTransform ToTransform() const {
        return FTransform(Rotation, Location, FVector(1.0f));
    }

    void FromTransform(const FTransform& Transform) {
        Location = Transform.GetLocation();
        Rotation = Transform.GetRotation();
    }
};

// 同步报告结构体
USTRUCT(BlueprintType)
struct FStringFlowSyncReport {
    GENERATED_BODY()

   public:
    UPROPERTY()
    bool bSuccess = true;

    UPROPERTY()
    TArray<FString> Warnings;

    UPROPERTY()
    TArray<FString> Errors;

    void AddWarning(const FString& Message) { Warnings.Add(Message); }

    void AddError(const FString& Message) {
        bSuccess = false;
        Errors.Add(Message);
    }

    void Clear() {
        bSuccess = true;
        Warnings.Empty();
        Errors.Empty();
    }
};

/**
 * AStringFlowUnreal - 小提琴动画系统的核心Actor类
 * 管理小提琴表演的控制器和记录器配置
 */
UCLASS(Blueprintable, BlueprintType)
class STRINGFLOWUNREAL_API AStringFlowUnreal : public AInstrumentBase, public FTickableGameObject {
    GENERATED_BODY()

   public:
    AStringFlowUnreal();

   protected:
    virtual void BeginPlay() override;

   public:
    virtual void Tick(float DeltaTime) override;

    // ========== 配置参数 ==========

    /** 每只手的手指数量（通常为4） */
    UPROPERTY(EditAnywhere, BlueprintReadWrite,
              Category = "StringFlow Configuration")
    int32 OneHandFingerNumber;

    /** 小提琴的弦数（通常为4） */
    UPROPERTY(EditAnywhere, BlueprintReadWrite,
              Category = "StringFlow Configuration")
    int32 StringNumber;

    /** 弦乐器模型（骨骼网格Actor） */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Basic Properties")
    class ASkeletalMeshActor* StringInstrument;

    /** 琴弓（骨骼网格Actor） */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Basic Properties")
    class ASkeletalMeshActor* Bow;

    // ========== 弦乐器特定配置 ==========

    /** 琴弓的朝向轴（X、Y、Z）用于指向弦触点 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Transform Sync")
    FVector BowAxisTowardString;

    /** 琴弓的向上轴（X、Y、Z）用于定义弓的上方向 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Transform Sync")
    FVector BowUpAxis;

    /** 是否启用实时同步弦乐器和琴弓的位置/旋转 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Transform Sync")
    bool bEnableRealtimeSync;

    /** 左手当前位置类型 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StringFlow State")
    EStringFlowLeftHandPositionType LeftHandPositionType;

    /** 右手当前位置类型 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StringFlow State")
    EStringFlowRightHandPositionType RightHandPositionType;

    /** 左手当前品格索引 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StringFlow State")
    EStringFlowLeftHandFretIndex LeftHandFretIndex;

    /** 右手当前弦索引 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StringFlow State")
    EStringFlowRightHandStringIndex RightHandStringIndex;

    /** 弦材质 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Basic Properties")
    class UMaterialInstance* StringMaterial;

    // ========== 控制器映射 ==========

    /** 左手手指控制器 */
    UPROPERTY()
    TMap<FString, FString> LeftFingerControllers;

    /** 右手手指控制器 */
    UPROPERTY()
    TMap<FString, FString> RightFingerControllers;

    /** 左手掌部控制器 */
    UPROPERTY()
    TMap<FString, FString> LeftHandControllers;

    /** 右手掌部控制器 */
    UPROPERTY()
    TMap<FString, FString> RightHandControllers;

    /** 其他控制器（触弦点、弓等） */
    UPROPERTY()
    TMap<FString, FString> OtherControllers;

    // ========== 记录器映射 ==========

    /** 左手手指记录器 */
    UPROPERTY()
    TMap<FString, FStringFlowStringArray> LeftFingerRecorders;

    /** 左手控制器位置记录器 */
    UPROPERTY()
    TMap<FString, FStringFlowStringArray> LeftHandPositionRecorders;

    /** 左手拇指控制器记录器 */
    UPROPERTY()
    TMap<FString, FStringFlowStringArray> LeftThumbRecorders;

    /** 右手手指记录器 */
    UPROPERTY()
    TMap<FString, FStringFlowStringArray> RightFingerRecorders;

    /** 右手控制器位置记录器 */
    UPROPERTY()
    TMap<FString, FStringFlowStringArray> RightHandPositionRecorders;

    /** 右手拇指控制器记录器 */
    UPROPERTY()
    TMap<FString, FStringFlowStringArray> RightThumbRecorders;

    /** 其他记录器（辅助点位） */
    UPROPERTY()
    TMap<FString, FStringFlowStringArray> OtherRecorders;

    /** 辅助线记录器 */
    UPROPERTY()
    TMap<FString, FString> GuideLines;

    // ========== 数据存储 ==========

    /** 记录器变换数据存储 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StringFlow Data")
    TMap<FString, FStringFlowRecorderTransform> RecorderTransforms;

    // ========== 方法声明 ==========

    /**
     * 获取手指控制器名称
     * @param FingerNumber 手指编号
     * @param HandType 手部类型
     * @return 控制器名称
     */
    UFUNCTION(BlueprintCallable, Category = "StringFlow")
    FString GetFingerControllerName(int32 FingerNumber,
                                    EStringFlowHandType HandType) const;

    /**
     * 获取左手手指记录器名称
     * @param StringIndex 弦索引
     * @param FretIndex 品格索引
     * @param FingerNumber 手指编号
     * @param PositionType 位置类型
     * @return 记录器名称
     */
    UFUNCTION(BlueprintCallable, Category = "StringFlow")
    FString GetLeftFingerRecorderName(int32 StringIndex, int32 FretIndex,
                                      int32 FingerNumber,
                                      const FString& PositionType) const;

    /**
     * 获取右手手指记录器名称（不包含品格信息）
     * @param StringIndex 弦索引
     * @param FingerNumber 手指编号
     * @param PositionType 位置类型
     * @return 记录器名称
     */
    UFUNCTION(BlueprintCallable, Category = "StringFlow")
    FString GetRightFingerRecorderName(int32 StringIndex, int32 FingerNumber,
                                       const FString& PositionType) const;

    /**
     * 获取手掌控制器名称
     * @param HandControllerType 手掌控制器类型
     * @param HandType 手部类型
     * @return 控制器名称
     */
    UFUNCTION(BlueprintCallable, Category = "StringFlow")
    FString GetHandControllerName(const FString& HandControllerType,
                                  EStringFlowHandType HandType) const;

    /**
     * 获取左手手掌记录器名称
     * @param StringIndex 弦索引
     * @param FretIndex 品格索引
     * @param HandControllerType 手掌控制器类型
     * @param PositionType 位置类型
     * @return 记录器名称
     */
    UFUNCTION(BlueprintCallable, Category = "StringFlow")
    FString GetLeftHandRecorderName(int32 StringIndex, int32 FretIndex,
                                    const FString& HandControllerType,
                                    const FString& PositionType) const;

    /**
     * 获取右手手掌记录器名称（不包含品格信息）
     * @param StringIndex 弦索引
     * @param HandControllerType 手掌控制器类型
     * @param PositionType 位置类型
     * @return 记录器名称
     */
    UFUNCTION(BlueprintCallable, Category = "StringFlow")
    FString GetRightHandRecorderName(int32 StringIndex,
                                     const FString& HandControllerType,
                                     const FString& PositionType) const;

    /**
     * 初始化所有控制器和记录器
     */
    UFUNCTION(BlueprintCallable, Category = "StringFlow")
    void InitializeControllersAndRecorders();

    /**
     * 获取位置类型字符串（左手）
     */
    UFUNCTION(BlueprintCallable, Category = "StringFlow")
    FString GetLeftHandPositionTypeString(
        EStringFlowLeftHandPositionType PositionType) const;

    /**
     * 获取位置类型字符串（右手）
     */
    UFUNCTION(BlueprintCallable, Category = "StringFlow")
    FString GetRightHandPositionTypeString(
        EStringFlowRightHandPositionType PositionType) const;

    /**
     * 导出记录器信息到JSON文件
     */
    UFUNCTION(BlueprintCallable, Category = "StringFlow")
    void ExportRecorderInfo(const FString& FilePath);

    /**
     * 从JSON文件导入记录器信息
     */
    UFUNCTION(BlueprintCallable, Category = "StringFlow")
    bool ImportRecorderInfo(const FString& FilePath);

#if WITH_EDITOR
    /**
     * 在编辑器中属性改变时调用，用于实时同步乐器位置
     */
    virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

    // ========== 已创建的对象 ==========

    /** 已创建的Actor对象映射 */
    UPROPERTY()
    TMap<FString, class AActor*> CreatedActors;

    /** 生成的小提琴材质 */
    UPROPERTY()
    TMap<FString, class UMaterialInstanceConstant*> GeneratedMaterials;

    // ========== FTickableGameObject 接口实现 ==========

    /**
     * 检查该对象是否可 Tick
     */
    virtual bool IsTickable() const override {
        return bEnableRealtimeSync;
    }

    /**
     * 检查该对象是否在编辑器中可 Tick
     */
    virtual bool IsTickableInEditor() const override {
        return bEnableRealtimeSync;
    }

    /**
     * 获取统计信息ID（用于性能分析）
     */
    virtual TStatId GetStatId() const override {
        RETURN_QUICK_DECLARE_CYCLE_STAT(AStringFlowUnreal, STATGROUP_Tickables);
    }

    /**
     * 获取性能框架信息
     */
    virtual bool IsAllowedToTick() const override {
        return true;
    }
};
