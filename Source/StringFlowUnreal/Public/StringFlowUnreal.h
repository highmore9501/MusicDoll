#pragma once

#include "Animation/SkeletalMeshActor.h"
#include "ControlRig/Public/ControlRig.h"
#include "ControlRigCacheSubsystem.h"
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "InstrumentBase.h"
#include "InstrumentControlRigUtility.h"
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

// ========== 乐器配置相关枚举和结构体 ==========

// 乐器类型枚举
UENUM(BlueprintType)
enum class EStringFlowInstrumentType : uint8 {
    VIOLIN = 0,  // 小提琴（默认）
    VIOLA = 1,   // 中提琴
    CELLO = 2,   // 大提琴
    CUSTOM = 3   // 自定义
};

// 乐器配置结构体
USTRUCT(BlueprintType)
struct FStringFlowInstrumentConfig {
    GENERATED_BODY()

   public:
    /** 乐器类型 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite,
              Category = "StringFlow Instrument")
    EStringFlowInstrumentType InstrumentType;

    /** 四根弦的MIDI音高 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite,
              Category = "StringFlow Instrument")
    TArray<int32> StringNotes;

    // 构造函数
    FStringFlowInstrumentConfig()
        : InstrumentType(EStringFlowInstrumentType::VIOLIN) {
        StringNotes.SetNum(4);
        StringNotes[0] = 76;  // E
        StringNotes[1] = 69;  // A
        StringNotes[2] = 62;  // D
        StringNotes[3] = 55;  // G
    }

    /**
     * 获取乐器名称
     */
    FString GetInstrumentName() const {
        switch (InstrumentType) {
            case EStringFlowInstrumentType::VIOLIN:
                return TEXT("Violin");
            case EStringFlowInstrumentType::VIOLA:
                return TEXT("Viola");
            case EStringFlowInstrumentType::CELLO:
                return TEXT("Cello");
            case EStringFlowInstrumentType::CUSTOM:
                return TEXT("Custom");
            default:
                return TEXT("Unknown");
        }
    }

    /**
     * 获取指定弦的音高
     */
    int32 GetStringNote(int32 StringIndex) const {
        if (StringIndex >= 0 && StringIndex < StringNotes.Num()) {
            return StringNotes[StringIndex];
        }
        return -1;
    }

    /**
     * 创建小提琴配置（E=76, A=69, D=62, G=55）
     */
    static FStringFlowInstrumentConfig GetViolinConfig() {
        FStringFlowInstrumentConfig Config;
        Config.InstrumentType = EStringFlowInstrumentType::VIOLIN;
        Config.StringNotes.SetNum(4);
        Config.StringNotes[0] = 76;  // E
        Config.StringNotes[1] = 69;  // A
        Config.StringNotes[2] = 62;  // D
        Config.StringNotes[3] = 55;  // G
        return Config;
    }

    /**
     * 创建中提琴配置（A=69, D=62, G=55, C=48）
     */
    static FStringFlowInstrumentConfig GetViolaConfig() {
        FStringFlowInstrumentConfig Config;
        Config.InstrumentType = EStringFlowInstrumentType::VIOLA;
        Config.StringNotes.SetNum(4);
        Config.StringNotes[0] = 69;  // A
        Config.StringNotes[1] = 62;  // D
        Config.StringNotes[2] = 55;  // G
        Config.StringNotes[3] = 48;  // C
        return Config;
    }

    /**
     * 创建大提琴配置（A=45, D=38, G=31, C=24）
     */
    static FStringFlowInstrumentConfig GetCelloConfig() {
        FStringFlowInstrumentConfig Config;
        Config.InstrumentType = EStringFlowInstrumentType::CELLO;
        Config.StringNotes.SetNum(4);
        Config.StringNotes[0] = 45;  // A
        Config.StringNotes[1] = 38;  // D
        Config.StringNotes[2] = 31;  // G
        Config.StringNotes[3] = 24;  // C
        return Config;
    }

    /**
     * 创建自定义配置
     */
    static FStringFlowInstrumentConfig GetCustomConfig(
        const TArray<int32>& InStringNotes) {
        FStringFlowInstrumentConfig Config;
        Config.InstrumentType = EStringFlowInstrumentType::CUSTOM;
        if (InStringNotes.Num() == 4) {
            Config.StringNotes = InStringNotes;
        }
        return Config;
    }
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

/**
 * AStringFlowUnreal - 小提琴动画系统的核心Actor类
 * 管理小提琴表演的控制器和记录器配置
 */
UCLASS(Blueprintable, BlueprintType)
class STRINGFLOWUNREAL_API AStringFlowUnreal : public AInstrumentBase,
                                               public FTickableGameObject {
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

    // ========== 弦乐器特定配置 ==========

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

    // ========== 渲染监控变量 ==========

    /** 弦材质 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Basic Properties")
    class UMaterialInstance* StringMaterial;

    // ========== 乐器配置 ==========

    /** 当前乐器配置 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite,
              Category = "StringFlow Configuration")
    FStringFlowInstrumentConfig CurrentInstrumentConfig;

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
     * 获取当前乐器的字符串名称
     */
    UFUNCTION(BlueprintCallable, Category = "StringFlow Instrument")
    FString GetCurrentInstrumentName() const;

    /**
     * 获取当前乐器的指定弦音高
     */
    UFUNCTION(BlueprintCallable, Category = "StringFlow Instrument")
    int32 GetCurrentStringNote(int32 StringIndex) const;

    /**
     * 设置乐器类型为小提琴
     */
    UFUNCTION(BlueprintCallable, Category = "StringFlow Instrument")
    void SetInstrumentToViolin();

    /**
     * 设置乐器类型为中提琴
     */
    UFUNCTION(BlueprintCallable, Category = "StringFlow Instrument")
    void SetInstrumentToViola();

    /**
     * 设置乐器类型为大提琴
     */
    UFUNCTION(BlueprintCallable, Category = "StringFlow Instrument")
    void SetInstrumentToCello();

    /**
     * 设置自定义乐器配置
     */
    UFUNCTION(BlueprintCallable, Category = "StringFlow Instrument")
    void SetCustomInstrumentConfig(const TArray<int32>& InStringNotes);

    /**
     * 导出记录器信息到JSON文件
     * @param FilePath 目标文件路径
     * @param bToBlender 为 true 时按 Blender 坐标系导出（位置 Y 取反、旋转 x/z
     * 取反）
     */
    UFUNCTION(BlueprintCallable, Category = "StringFlow")
    void ExportRecorderInfo(const FString& FilePath, bool bToBlender = false);

    /**
     * 从JSON文件导入记录器信息
     */
    UFUNCTION(BlueprintCallable, Category = "StringFlow")
    bool ImportRecorderInfo(const FString& FilePath);

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
    virtual bool IsTickable() const override { return true; }

    /**
     * 检查该对象是否在编辑器中可 Tick
     */
    virtual bool IsTickableInEditor() const override { return true; }

    /**
     * 获取统计信息ID（用于性能分析）
     */
    virtual TStatId GetStatId() const override {
        RETURN_QUICK_DECLARE_CYCLE_STAT(AStringFlowUnreal, STATGROUP_Tickables);
    }

    /**
     * 获取性能框架信息
     */
    virtual bool IsAllowedToTick() const override { return true; }

    /**
     * Called when the actor is being destroyed
     */
    virtual void BeginDestroy() override;

   private:
    /** 是否已完成初始化 */
    UPROPERTY(Transient)
    bool bIsInitialized;

   public:
    /**
     * 根据组件名称获取对应的SkeletalMeshActor
     * @param ComponentName 组件名称
     * @return 对应的SkeletalMeshActor
     */
    ASkeletalMeshActor* GetSkeletalMeshActorByName(FName ComponentName) const;

    /**
     * 获取指定组件的Control Rig实例（通过Subsystem）
     * @param ComponentName 组件名称
     * @return ControlRig实例
     */
    UControlRig* GetCachedControlRig(FName ComponentName);

    /**
     * 获取指定组件的Control Rig Blueprint（通过Subsystem）
     * @param ComponentName 组件名称
     * @return ControlRigBlueprint
     */
    UControlRigBlueprint* GetCachedControlRigBlueprint(FName ComponentName);

    /**
     * 当ControlRig操作失败时触发重新注册
     * @param ErrorMessage 错误信息，用于日志记录
     */
    void TriggerControlRigReregistration(const FString& ErrorMessage);

    /**
     * 检查是否已完成初始化
     * @return 是否已初始化
     */
    bool IsInitialized() const { return bIsInitialized; }
};
