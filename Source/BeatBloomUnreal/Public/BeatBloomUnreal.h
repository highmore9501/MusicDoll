#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "InstrumentBase.h"
#include "InstrumentControlRigUtility.h"
#include "Tickable.h"
#include "BeatBloomUnreal.generated.h"

/**
 * 鼓手状态枚举（对应 Blender 版 States 枚举）
 */
UENUM(BlueprintType)
enum class EBeatBloomState : uint8 {
    BEAT  UMETA(DisplayName = "Beat"),
    READY UMETA(DisplayName = "Ready"),
    REST  UMETA(DisplayName = "Rest")
};

/**
 * 肢体类型枚举
 */
UENUM(BlueprintType)
enum class EBeatBloomLimb : uint8 {
    LEFT_HAND  UMETA(DisplayName = "Left Hand"),
    RIGHT_HAND UMETA(DisplayName = "Right Hand"),
    LEFT_FOOT  UMETA(DisplayName = "Left Foot"),
    RIGHT_FOOT UMETA(DisplayName = "Right Foot")
};

/**
 * 记录器变换结构体
 */
USTRUCT(BlueprintType)
struct FBeatBloomRecorderTransform {
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FVector Location;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FQuat Rotation;

    FBeatBloomRecorderTransform()
        : Location(FVector::ZeroVector), Rotation(FQuat::Identity) {}
    FBeatBloomRecorderTransform(const FVector& InLocation,
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
 * 单个鼓件的可驱动肢体信息
 */
USTRUCT(BlueprintType)
struct FBeatBloomDrivableLimb {
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString Limb;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Coefficient = 1.0f;
};

/**
 * 单个鼓件的 MIDI 触发器
 */
USTRUCT(BlueprintType)
struct FBeatBloomMidiTrigger {
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 Note = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString Sound;
};

/**
 * 鼓件配置（对应 .drumkit 中的 components 数组元素）
 */
USTRUCT(BlueprintType)
struct FBeatBloomDrumComponent {
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString Name;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString Description;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bDoubleComponent = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FBeatBloomDrivableLimb> DrivableLimbs;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FBeatBloomMidiTrigger> MidiTriggers;
};

/**
 * 特殊动作配置（对应 .drumkit 中的 special_actions）
 */
USTRUCT(BlueprintType)
struct FBeatBloomSpecialAction {
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString Name;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString Description;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FString> Limbs;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FBeatBloomMidiTrigger> MidiTriggers;
};

/**
 * 完整鼓组配置（对应 .drumkit 文件）
 */
USTRUCT(BlueprintType)
struct FBeatBloomDrumKitConfig {
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString Name;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FString> Limbs;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FBeatBloomDrumComponent> Components;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FBeatBloomSpecialAction> SpecialActions;
};

/**
 * ABeatBloomUnreal - 打击乐动画系统的核心 Actor 类
 *
 * 管理鼓手的控制器和记录器配置，处理鼓组配置加载，
 * 以及设置数据的导入/导出。
 *
 * 与 FretDance 的核心差异：
 * - 四肢驱动（手+脚），而非仅双手
 * - 鼓件配置由 .drumkit 文件动态加载，非硬编码
 * - 有目标控制器（Tar_Body/Chest/Head）驱动身体朝向
 * - 记录器按鼓件名+状态动态命名
 */
UCLASS(Blueprintable, BlueprintType)
class BEATBLOOMUNREAL_API ABeatBloomUnreal : public AInstrumentBase,
                                             public FTickableGameObject {
    GENERATED_BODY()

public:
    ABeatBloomUnreal();

    // FTickableGameObject interface
    virtual void Tick(float DeltaTime) override;
    virtual bool IsTickable() const override { return true; }
    virtual bool IsTickableInEditor() const override { return true; }
    virtual bool IsTickableWhenPaused() const override { return true; }
    virtual TStatId GetStatId() const override {
        RETURN_QUICK_DECLARE_CYCLE_STAT(ABeatBloomUnreal, STATGROUP_Tickables);
    }

protected:
    virtual void BeginPlay() override;

public:
    // ============ 乐器骨骼引用 ============

    /** 鼓组骨骼 Mesh */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Basic Properties")
    ASkeletalMeshActor* DrumKit;

    // ============ 实时同步开关 ============

    /** 开启实时同步（鼓组跟随演奏者） */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Transform Sync")
    bool bEnableRealtimeSync;

    // ============ IO 配置 ============

    /** 鼓组配置文件路径（.drumkit） */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "IO Configuration")
    FString DrumKitConfigPath;

    // ============ 当前 UI 选择状态 ============

    /** 左手当前选择的鼓件名 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Current State")
    FString CurrentLeftHandDrumKit;

    /** 右手当前选择的鼓件名 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Current State")
    FString CurrentRightHandDrumKit;

    /** 左脚当前选择的鼓件名 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Current State")
    FString CurrentLeftFootDrumKit;

    /** 右脚当前选择的鼓件名 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Current State")
    FString CurrentRightFootDrumKit;

    /** 目标控制器当前选择的鼓件名 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Current State")
    FString CurrentTargetDrumKit;

    /** 左手当前状态 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Current State")
    EBeatBloomState CurrentLeftHandState;

    /** 右手当前状态 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Current State")
    EBeatBloomState CurrentRightHandState;

    /** 左脚当前状态 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Current State")
    EBeatBloomState CurrentLeftFootState;

    /** 右脚当前状态 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Current State")
    EBeatBloomState CurrentRightFootState;

    /** 目标控制器当前状态 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Current State")
    EBeatBloomState CurrentTargetState;

    // ============ 控制器映射（固定） ============

    /** 手部控制器映射：描述名 -> ControlRig 控制器名 */
    UPROPERTY()
    TMap<FString, FString> HandControllers;

    /** 脚部控制器映射：描述名 -> ControlRig 控制器名 */
    UPROPERTY()
    TMap<FString, FString> FootControllers;

    /** 目标控制器映射：描述名 -> ControlRig 控制器名 */
    UPROPERTY()
    TMap<FString, FString> TargetControllers;

    // ============ 记录器映射（基于 drumkit 配置动态生成） ============

    /** 手部记录器映射：记录器名 -> 记录器名（自映射，用于快速查找） */
    UPROPERTY()
    TMap<FString, FString> HandRecorders;

    /** 脚部记录器映射 */
    UPROPERTY()
    TMap<FString, FString> FootRecorders;

    /** 目标记录器映射 */
    UPROPERTY()
    TMap<FString, FString> TargetRecorders;

    // ============ 数据存储 ============

    /** 所有记录器的变换数据 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BeatBloom Data")
    TMap<FString, FBeatBloomRecorderTransform> RecorderTransforms;

    /** 当前加载的鼓组配置 */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "BeatBloom Data")
    FBeatBloomDrumKitConfig DrumKitConfig;

    // ============ 同步缓存 ============

    /** 鼓组同步的相对变换缓存 */
    UPROPERTY(VisibleAnywhere, Category = "Transform Sync Cache")
    FTransform CachedDrumKitRelativeTransform;

    // ============ 核心方法 ============

    /**
     * 初始化控制器映射（固定映射，不依赖鼓组配置）
     * 设置 HandControllers、FootControllers、TargetControllers
     */
    void InitializeControllersAndRecorders();

    /**
     * 加载鼓组配置文件（.drumkit）
     * 解析 JSON 并填充 DrumKitConfig 结构体，然后调用 InitializeRecordersFromConfig
     *
     * @param FilePath .drumkit 文件路径
     * @return 加载是否成功
     */
    bool LoadDrumKitConfig(const FString& FilePath);

    /**
     * 根据已加载的 DrumKitConfig 初始化记录器映射
     * 动态生成 HandRecorders、FootRecorders、TargetRecorders
     * 并初始化 RecorderTransforms 的默认值
     */
    void InitializeRecordersFromConfig();

    /**
     * 导出记录器信息到 .drummer 文件
     *
     * @param FilePath 目标文件路径
     */
    void ExportRecorderInfo(const FString& FilePath);

    /**
     * 从 .drummer 文件导入记录器信息
     *
     * @param FilePath 源文件路径
     * @return 导入是否成功
     */
    bool ImportRecorderInfo(const FString& FilePath);

    /**
     * 获取指定肢体可驱动的鼓件选项列表
     *
     * @param Limb 肢体类型
     * @return 鼓件名称数组
     */
    TArray<FString> GetDrumKitOptionsForLimb(EBeatBloomLimb Limb) const;

    /**
     * 获取目标控制器可用的鼓件选项列表（手部可驱动的组件+特殊动作+休息）
     *
     * @return 鼓件名称数组
     */
    TArray<FString> GetTargetDrumKitOptions() const;

    /**
     * 获取当前所有控制器到记录器的完整映射
     * 基于当前 UI 选择状态（CurrentXxxDrumKit + CurrentXxxState）
     *
     * @return 控制器名 -> 记录器名 映射
     */
    TMap<FString, FString> GetCurrentControllerToRecorderMapping() const;

    /**
     * 获取状态名称字符串
     *
     * @param State 状态枚举
     * @return 状态字符串（"beat", "ready", "rest"）
     */
    static FString GetStateString(EBeatBloomState State);

    /**
     * 获取肢体名称字符串
     *
     * @param Limb 肢体枚举
     * @return 肢体字符串（"left_hand", "right_hand", "left_foot", "right_foot"）
     */
    static FString GetLimbString(EBeatBloomLimb Limb);

    // ============ ControlRig 缓存访问 ============

    /** 获取缓存的 ControlRig 实例 */
    UControlRig* GetCachedControlRig(FName ComponentName);

    /** 获取缓存的 ControlRig Blueprint */
    UControlRigBlueprint* GetCachedControlRigBlueprint(FName ComponentName);

    /** 注册所有 ControlRig 到缓存子系统 */
    void RegisterAllControlRigs();

    /** 当 ControlRig 操作失败时触发重新注册 */
    void TriggerControlRigReregistration(const FString& ErrorMessage);

    /** 检查是否已初始化 */
    bool IsInitialized() const { return bIsInitialized; }

    // ============ 动画文件解析 ============

    /**
     * 解析 .beatbloom 文件，提取演奏者和鼓组动画路径
     *
     * @param FilePath .beatbloom 文件的完整路径
     * @param OutPerformerAnimationPath 输出：演奏者动画路径
     * @param OutDrumKitAnimationPath 输出：鼓组 ShapeKey 动画路径
     * @return 解析是否成功
     */
    static bool ParseBeatBloomFile(
        const FString& FilePath,
        FString& OutPerformerAnimationPath,
        FString& OutDrumKitAnimationPath);

private:
    // ============ 辅助方法 ============
    
    /** 为单个鼓组件生成所有状态和肢体的记录器 */
    void GenerateRecordersForComponent(
        const FString& ComponentName,
        const TArray<FBeatBloomDrivableLimb>& DrivableLimbs);
    
    /** 添加休息状态记录器（手部专用） */
    void AddRestRecorders();
    
    /** 添加目标控制器记录器（仅手部可驱动的组件） */
    void AddTargetRecorders();
    
    /** 添加单个记录器到映射中 */
    void AddRecorder(const FString& RecorderName);
    
    /** 是否已完成初始化 */
    UPROPERTY(Transient)
    bool bIsInitialized;
};
