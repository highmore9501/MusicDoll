#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "InstrumentBase.h"
#include "InstrumentControlRigUtility.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Tickable.h"
#include "ZhengDriftUnreal.generated.h"

// ============================================================
// 枚举定义
// ============================================================

UENUM(BlueprintType)
enum class EZhengDriftHandType : uint8 { LEFT = 0, RIGHT = 1 };

UENUM(BlueprintType)
enum class EZhengDriftLeftHandAction : uint8 {
    NORMAL = 0,  // 普通拨弦
    PRESS = 1    // 按弦（左手压弦）
};

UENUM(BlueprintType)
enum class EZhengDriftRightHandAction : uint8 {
    NORMAL = 0,   // 普通拨弦
    TREMOLO = 1   // 摇指
};

UENUM(BlueprintType)
enum class EZhengDriftHandPosition : uint8 {
    FAR = 0,     // 远端（靠近第 0 弦区域）
    MIDDLE = 1,  // 中间（第 10 弦区域）
    NEAR = 2     // 近端（第 20 弦区域）
};

// ============================================================
// 辅助结构体
// ============================================================

// 记录器变换结构体（类比 FFretDanceRecorderTransform）
USTRUCT(BlueprintType)
struct FZhengDriftRecorderTransform {
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FVector Location;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FQuat Rotation;

    FZhengDriftRecorderTransform()
        : Location(FVector::ZeroVector), Rotation(FQuat::Identity) {}

    FZhengDriftRecorderTransform(const FVector& InLocation,
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

// ============================================================
// 核心 Actor 类
// ============================================================

/**
 * AZhengDriftUnreal — 古筝动画系统核心 Actor
 *
 * 古筝参数：
 * - 21 根弦（索引 0-20）
 * - 左手动作：Normal / Press
 * - 右手动作：Normal / Tremolo
 * - 手部位置：Far / Middle / Near
 * - 无品格概念
 *
 * 控制器总数：35 个
 *   左手 12 个：H_L, HP_L, T_L, TP_L, I_L, M_L, R_L, P_L,
 *               I_L_pole, M_L_pole, R_L_pole, P_L_pole
 *   右手 12 个：同上 _R
 *   脚部  4 个：F_L, F_L_pole, F_R, F_R_pole
 *   Target 3 个：Middle_Hand, Look_At, Head_Control
 *   双线性辅助 8 个：Middle_Hand_A/B/C/D, Head_Control_A/B/C/D
 */
UCLASS(Blueprintable, BlueprintType)
class ZHENGDRIFTUNREAL_API AZhengDriftUnreal : public AInstrumentBase,
                                               public FTickableGameObject {
    GENERATED_BODY()

public:
    AZhengDriftUnreal();

    // FTickableGameObject interface
    virtual void Tick(float DeltaTime) override;
    virtual bool IsTickable() const override { return true; }
    virtual bool IsTickableInEditor() const override { return true; }
    virtual bool IsTickableWhenPaused() const override { return true; }
    virtual TStatId GetStatId() const override {
        RETURN_QUICK_DECLARE_CYCLE_STAT(AZhengDriftUnreal, STATGROUP_Tickables);
    }

protected:
    virtual void BeginPlay() override;

public:
    // ========== 乐器引用 ==========

    /** 古筝骨骼模型 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Basic Properties")
    ASkeletalMeshActor* Zheng;

    // ========== 配置 ==========

    /** 弦数（固定 21） */
    UPROPERTY(EditAnywhere, BlueprintReadWrite,
              Category = "ZhengDrift Configuration")
    int32 StringNumber;

    // ========== 当前状态 ==========

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ZhengDrift State")
    EZhengDriftHandPosition CurrentLeftHandPosition;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ZhengDrift State")
    EZhengDriftLeftHandAction CurrentLeftHandAction;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ZhengDrift State")
    EZhengDriftHandPosition CurrentRightHandPosition;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ZhengDrift State")
    EZhengDriftRightHandAction CurrentRightHandAction;

    // ========== 控制器映射 ==========

    /** 左手控制器：key=内部名称, value=Control Rig 名称（12 个） */
    UPROPERTY()
    TMap<FString, FString> LeftHandControllers;

    /** 右手控制器：12 个 */
    UPROPERTY()
    TMap<FString, FString> RightHandControllers;

    /** 脚部控制器：4 个 */
    UPROPERTY()
    TMap<FString, FString> FootControllers;

    /** Target 控制器：3 个（特殊朝向控制器，无对应记录器） */
    UPROPERTY()
    TMap<FString, FString> TargetControllers;

    /** 双线性映射辅助控制器：8 个（Middle_Hand_A/B/C/D + Head_Control_A/B/C/D，只持久化 location） */
    UPROPERTY()
    TMap<FString, FString> BilinearHelpers;

    // ========== 记录器映射 ==========

    /** 弦位置记录器：63 个（21弦 × 3点：head/end/mid） */
    UPROPERTY()
    TMap<FString, FString> StringPositionRecorders;

    /** 左手记录器：48 个（3位置 × 2动作 × 8手指） */
    UPROPERTY()
    TMap<FString, FString> LeftHandRecorders;

    /** 右手记录器：48 个（3位置 × 2动作 × 8手指） */
    UPROPERTY()
    TMap<FString, FString> RightHandRecorders;



    // ========== 数据存储 ==========

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ZhengDrift Data")
    TMap<FString, FZhengDriftRecorderTransform> RecorderTransforms;

    /** 古筝第创建的材质实例缓存（16 根弦的独立材质） */
    UPROPERTY()
    TMap<FString, UMaterialInstanceConstant*> GeneratedMaterials;

    // ========== 核心初始化 ==========

    /**
     * 初始化所有控制器和记录器映射
     * 在构造函数中调用，直接翻译自 Blender 插件 ZhengBaseState.__init__
     */
    void InitializeControllersAndRecorders();

    // ========== 状态映射方法 ==========

    /**
     * 获取左手 Controller → Recorder 映射（用于 SaveState）
     * 自动排除 pole 型控制器
     */
    TMap<FString, FString> GetLeftHandControllerToRecorderMapping(
        EZhengDriftHandPosition Position,
        EZhengDriftLeftHandAction Action) const;

    /** 获取右手 Controller → Recorder 映射 */
    TMap<FString, FString> GetRightHandControllerToRecorderMapping(
        EZhengDriftHandPosition Position,
        EZhengDriftRightHandAction Action) const;

    /** 反向映射：Recorder → Controller（用于 LoadState） */
    TMap<FString, FString> GetLeftHandRecorderToControllerMapping(
        EZhengDriftHandPosition Position,
        EZhengDriftLeftHandAction Action) const;

    TMap<FString, FString> GetRightHandRecorderToControllerMapping(
        EZhengDriftHandPosition Position,
        EZhengDriftRightHandAction Action) const;

    // ========== ControlRig 缓存 ==========

    UControlRig* GetCachedControlRig(FName ComponentName);
    UControlRigBlueprint* GetCachedControlRigBlueprint(FName ComponentName);
    void RegisterAllControlRigs();
    void TriggerControlRigReregistration(const FString& ErrorMessage);

    /**
     * 根据组件名获取对应的 SkeletalMeshActor
     * "Zheng" → Zheng, "Performer" → SkeletalMeshActor
     */
    ASkeletalMeshActor* GetSkeletalMeshActorByName(FName ComponentName) const;

    // ========== 导入/导出 ==========

    void ExportRecorderInfo(const FString& FilePath);
    bool ImportRecorderInfo(const FString& FilePath);

    // ========== 静态辅助 ==========

    static FString GetHandPositionString(EZhengDriftHandPosition Position);
    static FString GetLeftHandActionString(EZhengDriftLeftHandAction Action);
    static FString GetRightHandActionString(EZhengDriftRightHandAction Action);
};
