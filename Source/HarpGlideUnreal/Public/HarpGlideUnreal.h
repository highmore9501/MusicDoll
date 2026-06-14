#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "InstrumentBase.h"
#include "InstrumentControlRigUtility.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Tickable.h"
#include "HarpGlideUnreal.generated.h"

// ============================================================
// 枚举定义
// ============================================================

UENUM(BlueprintType)
enum class EHarpGlideHandType : uint8 { LEFT = 0, RIGHT = 1 };

UENUM(BlueprintType)
enum class EHarpGlideHandPose : uint8 {
    FAR = 0,     // 常规演奏远端
    NEAR = 1,    // 常规演奏近端
    ATTACK = 2,  // 拨弦瞬间
    REST = 3     // 完成后休息姿�?
};

UENUM(BlueprintType)
enum class EHarpGlidePedalNote : uint8 {
    D = 0,
    C = 1,
    B = 2,  // 左脚控制（F_L�?
    E = 3,
    F = 4,
    G = 5,
    A = 6  // 右脚控制（F_R�?
};

UENUM(BlueprintType)
enum class EHarpGlidePedalState : uint8 {
    FLAT = 0,     // �?降音位置
    STATE_1 = 1,  // 中降位置
    NATURAL = 2,  // �?原音位置
    STATE_3 = 3,  // 中升位置
    SHARP = 4     // �?升音位置
};

UENUM(BlueprintType)
enum class EHarpGlideTiltState : uint8 {
    NEAR = 0,  // 近端（向演奏者倾斜�?
    MID = 1,   // 中间位置
    FAR = 2    // 远端（向远处倾斜�?
};

// ============================================================
// 辅助结构�?
// ============================================================

USTRUCT(BlueprintType)
struct FHarpGlideRecorderTransform {
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FVector Location;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FQuat Rotation;

    FHarpGlideRecorderTransform()
        : Location(FVector::ZeroVector), Rotation(FQuat::Identity) {}

    FHarpGlideRecorderTransform(const FVector& InLocation,
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
// 核心 Actor �?
// ============================================================

/**
 * AHarpGlideUnreal �?竖琴动画系统核心 Actor
 *
 * 竖琴参数�?
 * - 47 根弦（索�?0-46�?
 * - 双手均仅拨弦（无按弦�?
 * - 手部姿势：Far / Near / Attack / Rest�? 态）
 * - 7 个踏板，�?5 档位（Flat ~ Sharp�?
 * - 竖琴单支点三态倾斜（Near / Mid / Far�?
 * - 无品格概�?
 *
 * 控制器总数�?2 �?
 *   身体  2 个：Head, Shoulder_Harp
 *   左手  7 个：H_L, HP_L, T_L, I_L, M_L, R_L, P_L
 *   右手  7 个：H_R, HP_R, T_R, I_R, M_R, R_R, P_R
 *   脚部  4 个：F_L, FP_L, F_R, FP_R
 *   Target 3 个：Mid_Hand, Look_At
 *   竖琴支点 1 个：harp_pivot
 *   手指极向�?10 个：T/I/M/R/P_L_pole, T/I/M/R/P_R_pole
 *
 * 记录器总数�?94 �?
 *   弦位�?94 个（47�?× 2�?head/end�?
 *   踏板位置 35 个（7踏板 × 5档位�?
 *   支点状�?3 个（near/mid/far�?
 *   左手姿势 28 个（7控制�?× 4姿势�?
 *   右手姿势 28 个（7控制�?× 4姿势�?
 *   头部姿势 4 �?
 *   脚部休息 2 �?
 */
UCLASS(Blueprintable, BlueprintType)
class HARPGLIDEUNREAL_API AHarpGlideUnreal : public AInstrumentBase,
                                             public FTickableGameObject {
    GENERATED_BODY()

   public:
    AHarpGlideUnreal();

    // FTickableGameObject interface
    virtual void Tick(float DeltaTime) override;
    virtual bool IsTickable() const override { return true; }
    virtual bool IsTickableInEditor() const override { return true; }
    virtual bool IsTickableWhenPaused() const override { return true; }
    virtual TStatId GetStatId() const override {
        RETURN_QUICK_DECLARE_CYCLE_STAT(AHarpGlideUnreal, STATGROUP_Tickables);
    }

   protected:
    virtual void BeginPlay() override;

   public:
    // ========== 乐器引用 ==========

    /** 竖琴骨骼模型（用于竖�?Control Rig，Morph Target 载体�?*/
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Basic Properties")
    ASkeletalMeshActor* Harp;

    // ========== 配置 ==========

    /** 弦数（默�?47�?*/
    UPROPERTY(EditAnywhere, BlueprintReadWrite,
              Category = "HarpGlide Configuration")
    int32 StringNumber;

    /** 左手远端位置参数�?~100�?*/
    UPROPERTY(EditAnywhere, BlueprintReadWrite,
              Category = "HarpGlide Configuration")
    int32 LeftFar;

    /** 左手近端位置参数�?~100�?*/
    UPROPERTY(EditAnywhere, BlueprintReadWrite,
              Category = "HarpGlide Configuration")
    int32 LeftNear;

    /** 左手中远位置参数�?~100�?*/
    UPROPERTY(EditAnywhere, BlueprintReadWrite,
              Category = "HarpGlide Configuration")
    int32 LeftMidFar;

    /** 左手中近位置参数�?~100�?*/
    UPROPERTY(EditAnywhere, BlueprintReadWrite,
              Category = "HarpGlide Configuration")
    int32 LeftMidNear;

    /** 右手远端位置参数�?~100�?*/
    UPROPERTY(EditAnywhere, BlueprintReadWrite,
              Category = "HarpGlide Configuration")
    int32 RightFar;

    /** 右手近端位置参数�?~100�?*/
    UPROPERTY(EditAnywhere, BlueprintReadWrite,
              Category = "HarpGlide Configuration")
    int32 RightNear;

    /** 标记�?Unreal 导出（始终为 true，写�?.harpist �?Blender 识别�?*/
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly,
              Category = "HarpGlide Configuration")
    bool bIsUnreal;

    // ========== 当前状�?==========

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HarpGlide State")
    EHarpGlideHandPose CurrentLeftHandPose;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HarpGlide State")
    EHarpGlideHandPose CurrentRightHandPose;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HarpGlide State")
    EHarpGlideTiltState CurrentTiltState;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HarpGlide State")
    EHarpGlidePedalNote CurrentPedalNote;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HarpGlide State")
    EHarpGlidePedalState CurrentPedalState;

    // ========== 控制器映�?==========

    /** 身体控制器：2 �?*/
    UPROPERTY()
    TMap<FString, FString> BodyControllers;

    /** 左手控制器：7 个（key=内部�? value=CR 名称�?*/
    UPROPERTY()
    TMap<FString, FString> LeftHandControllers;

    /** 右手控制器：7 �?*/
    UPROPERTY()
    TMap<FString, FString> RightHandControllers;

    /** 脚部控制器：4 �?*/
    UPROPERTY()
    TMap<FString, FString> FootControllers;

    /** Target 控制器：3 个（特殊朝向控制器） */
    UPROPERTY()
    TMap<FString, FString> TargetControllers;

    /** 竖琴支点控制器：1 个（harp_pivot，驱�?harp 骨骼�?*/
    UPROPERTY()
    TMap<FString, FString> HarpPivotControllers;

    /** 手指极向量控制器�?0 个（仅手动调节，不参�?Save/Load�?*/
    UPROPERTY()
    TMap<FString, FString> HandPoleControllers;

    /** 双线性映射辅助控制器�? 个（只持久化 location�?*/
    UPROPERTY()
    TMap<FString, FString> BilinearHelpers;

    // ========== 记录器映�?==========

    /** 弦位置记录器�?4 个（47�?× 2�?head/end�?*/
    UPROPERTY()
    TMap<FString, FString> StringPositionRecorders;

    /** 踏板位置记录器：35 个（7踏板 × 5档位�?*/
    UPROPERTY()
    TMap<FString, FString> PedalPositionRecorders;

    /** 竖琴支点状态记录器�? 个（near/mid/far�?*/
    UPROPERTY()
    TMap<FString, FString> HarpPivotRecorders;

    /** 左手姿势记录器：28 个（7控制�?× 4姿势�?*/
    UPROPERTY()
    TMap<FString, FString> LeftHandRecorders;

    /** 右手姿势记录器：28 �?*/
    UPROPERTY()
    TMap<FString, FString> RightHandRecorders;

    /** 头部姿势记录器：4 个（1控制�?× 4姿势�?*/
    UPROPERTY()
    TMap<FString, FString> HeadRecorders;

    /** 脚部休息记录器：2 �?*/
    UPROPERTY()
    TMap<FString, FString> FootRestRecorders;

    // ========== 数据存储 ==========

    /** 统一记录器变换数据存�?*/
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HarpGlide Data")
    TMap<FString, FHarpGlideRecorderTransform> RecorderTransforms;

    /** 竖琴弦材质实例缓存（47 根弦的独立材质） */
    UPROPERTY()
    TMap<FString, UMaterialInstanceConstant*> GeneratedMaterials;

    // ========== 核心初始�?==========

    /**
     * 初始化所有控制器和记录器映射
     * 在构造函数中调用，直接翻译自 Blender 插件 HarpBaseState.__init__
     */
    void InitializeControllersAndRecorders();

    // ========== 状态映射方�?==========

    /**
     * 获取左手 Controller �?Recorder 映射（用�?SaveState�?
     * 自动排除 pole 型控制器�?HandPoleControllers
     *
     * @param Pose 手部姿势（Far/Near/Attack/Rest�?
     * @return {控制器内部键 �?记录器名} 映射
     */
    TMap<FString, FString> GetLeftHandControllerToRecorderMapping(
        EHarpGlideHandPose Pose) const;

    /** 获取右手 Controller �?Recorder 映射 */
    TMap<FString, FString> GetRightHandControllerToRecorderMapping(
        EHarpGlideHandPose Pose) const;

    /** 反向映射：Recorder �?Controller（用�?LoadState�?*/
    TMap<FString, FString> GetLeftHandRecorderToControllerMapping(
        EHarpGlideHandPose Pose) const;

    TMap<FString, FString> GetRightHandRecorderToControllerMapping(
        EHarpGlideHandPose Pose) const;

    // ========== ControlRig 缓存 ==========

    UControlRig* GetCachedControlRig(FName ComponentName);
    UControlRigBlueprint* GetCachedControlRigBlueprint(FName ComponentName);
    void RegisterAllControlRigs();
    void TriggerControlRigReregistration(const FString& ErrorMessage);

    /**
     * 根据组件名获取对应的 SkeletalMeshActor
     * "Harp" �?Harp, "Performer" �?SkeletalMeshActor
     */
    ASkeletalMeshActor* GetSkeletalMeshActorByName(FName ComponentName) const;

    // ========== 导入/导出 ==========

    void ExportRecorderInfo(const FString& FilePath);
    bool ImportRecorderInfo(const FString& FilePath);

    // ========== 静态辅�?==========

    static FString GetHandPoseString(EHarpGlideHandPose Pose);
    static FString GetPedalNoteString(EHarpGlidePedalNote Note);
    static FString GetPedalStateString(EHarpGlidePedalState State);
    static FString GetTiltStateString(EHarpGlideTiltState State);
};
