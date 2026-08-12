#pragma once

#include "ControlRig/Public/ControlRig.h"
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "InstrumentBase.h"
#include "InstrumentControlRigUtility.h"
#include "Tickable.h"
#include "FretDanceUnreal.generated.h"

// 手部枚举
UENUM(BlueprintType)
enum class EFretDanceHandType : uint8 { LEFT = 0, RIGHT = 1 };

// 吉他类型枚举
UENUM(BlueprintType)
enum class EFretDanceInstrumentType : uint8 {
    FINGER_STYLE_GUITAR = 0,  // 指弹吉他
    ELECTRIC_GUITAR = 1,      // 电吉他
    BASS = 2                  // 贝斯
};

/**
 * 乐器类型说明：
 * - 控制器定义：所有乐器类型都使用相同的右手手指控制器(IMRP + T)
 * - 主要差异：体现在Control Rig的层级结构上
 * - 电吉他特殊处理：右手H(手掌)与T(大拇指)同级，I(食指)是T的子级，MRP是H的子级
 * - 其他乐器：IMRPT五个手指以及H(手掌)都是同一级
 */

// 左手基础位置枚举
UENUM(BlueprintType)
enum class EFretDanceBasePosition : uint8 {
    P0 = 0,
    P1 = 1,
    P2 = 2,
    P3 = 3,
    P4 = 4
};

// 左手状态枚举
UENUM(BlueprintType)
enum class EFretDanceLeftHandState : uint8 {
    NORMAL = 0,
    OUTER = 1,
    INNER = 2,
    BARRE = 3
};

// 右手状态枚举
UENUM(BlueprintType)
enum class EFretDanceRightHandState : uint8 {
    LOW = 0,      // 值 "0"
    END = 1,      // 值 "end"
    HIGH = 2,     // 值 "3"
    RELEASE = 3,  // 颤音摇杆-松开
    UP = 4,       // 颤音摇杆-上摇
    DOWN = 5      // 颤音摇杆-下压
};

// 字符串数组结构体
USTRUCT(BlueprintType)
struct FFretDanceStringArray {
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FString> Strings;

    void Add(const FString& String) { Strings.Add(String); }
    int32 Num() const { return Strings.Num(); }
    const FString& operator[](int32 Index) const { return Strings[Index]; }
    FString& operator[](int32 Index) { return Strings[Index]; }
};

// 记录器变换结构体
USTRUCT(BlueprintType)
struct FFretDanceRecorderTransform {
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FVector Location;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FQuat Rotation;

    FFretDanceRecorderTransform()
        : Location(FVector::ZeroVector), Rotation(FQuat::Identity) {}
    FFretDanceRecorderTransform(const FVector& InLocation,
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
struct FFretDanceSyncReport {
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bSuccess;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString Message;

    FFretDanceSyncReport() : bSuccess(false) {}
    FFretDanceSyncReport(bool InSuccess, const FString& InMessage)
        : bSuccess(InSuccess), Message(InMessage) {}
};

// 无效左手状态组合结构体
USTRUCT(BlueprintType)
struct FFretDanceInvalidLeftHandCombinations {
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TSet<EFretDanceLeftHandState> InvalidStates;

    void AddInvalidState(EFretDanceLeftHandState State) {
        InvalidStates.Add(State);
    }
    bool Contains(EFretDanceLeftHandState State) const {
        return InvalidStates.Contains(State);
    }
    bool IsEmpty() const { return InvalidStates.IsEmpty(); }
    int32 Num() const { return InvalidStates.Num(); }
};

/**
 * AFretDanceUnreal - 吉他动画系统的核心Actor类
 * 管理吉他演奏的控制器和记录器配置
 */
UCLASS(Blueprintable, BlueprintType)
class FRETDANCEUNREAL_API AFretDanceUnreal : public AInstrumentBase,
                                             public FTickableGameObject {
    GENERATED_BODY()

   public:
    AFretDanceUnreal();

    // FTickableGameObject interface
    virtual void Tick(float DeltaTime) override;
    virtual bool IsTickable() const override { return true; }
    virtual bool IsTickableInEditor() const override { return true; }
    virtual bool IsTickableWhenPaused() const override { return true; }
    virtual TStatId GetStatId() const override {
        RETURN_QUICK_DECLARE_CYCLE_STAT(AFretDanceUnreal, STATGROUP_Tickables);
    }

   protected:
    // Called when the game starts or when spawned
    virtual void BeginPlay() override;

   public:
    // 乐器类型
    UPROPERTY(EditAnywhere, BlueprintReadWrite,
              Category = "FretDance Configuration")
    EFretDanceInstrumentType InstrumentType;

    // 弦数
    UPROPERTY(EditAnywhere, BlueprintReadWrite,
              Category = "FretDance Configuration")
    int32 StringNumber;

    // 吉它模型
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Basic Properties")
    ASkeletalMeshActor* Guitar;

    // 左手位置
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FretDance State")
    EFretDanceBasePosition CurrentBasePosition;
    // 左手状态
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FretDance State")
    EFretDanceLeftHandState CurrentLeftHandState;
    // 右手位置
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FretDance State")
    EFretDanceRightHandState CurrentRightHandState;

    // 是否使用颤音摇杆（仅电吉他可用）
    UPROPERTY(EditAnywhere, BlueprintReadWrite,
              Category = "FretDance Configuration")
    bool bUseVibratoBar = false;

    // ========== 控制器映射 ==========
    // 左手手掌控制器 (H_L, HP_L, T_L)
    UPROPERTY()
    TMap<FString, FString> LeftHandControllers;

    // 右手手掌控制器 (H_R, HP_R)
    UPROPERTY()
    TMap<FString, FString> RightHandControllers;

    // 左手手指控制器 (I_L, M_L, R_L, P_L)
    UPROPERTY()
    TMap<FString, FString> LeftFingerControllers;

    // 右手手指控制器（因乐器类型不同而变化）
    UPROPERTY()
    TMap<FString, FString> RightFingerControllers;

    // ========== 记录器映射 ==========
    // 左手手指记录器
    UPROPERTY()
    TMap<FString, FFretDanceStringArray> LeftFingerRecorders;

    // 右手手指记录器
    UPROPERTY()
    TMap<FString, FFretDanceStringArray> RightFingerRecorders;

    // 其他记录器
    UPROPERTY()
    TMap<FString, FFretDanceStringArray> OtherRecorders;

    // 左手位置记录器 (包含位置和旋转)
    UPROPERTY()
    TMap<FString, FFretDanceStringArray> LeftHandPositionRecorders;

    // 右手位置记录器
    UPROPERTY()
    TMap<FString, FFretDanceStringArray> RightHandPositionRecorders;

    // 指板位置记录器
    UPROPERTY()
    TMap<FString, FString> GuitarFretPositions;

    // ========== 数据存储 ==========
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FretDance Data")
    TMap<FString, FFretDanceRecorderTransform> RecorderTransforms;

    // ========== 弦材质 ==========
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Basic Properties")
    UMaterialInstance* StringMaterial;

   private:
    /** 是否已完成初始化 */
    UPROPERTY(Transient)
    bool bIsInitialized;

   public:
    // ========== 核心初始化方法 ==========
    void InitializeControllersAndRecorders();

    // 根据当前乐器类型获取右手手指控制器列表
    TMap<FString, FString> GetRightFingerControllersForInstrumentType() const;

    // 获取记录器名称（左手）- 单向映射：Controller -> Recorder
    FString GetLeftHandRecorderName(EFretDanceBasePosition Position,
                                    EFretDanceLeftHandState State,
                                    const FString& ControllerName) const;

    // 获取记录器名称（右手）- 单向映射：FingerKey -> Recorder
    FString GetRightHandRecorderName(EFretDanceRightHandState State,
                                     const FString& FingerKey) const;

    // 获取左手 Controller 到 Recorder 的完整映射（用于 SaveState）
    TMap<FString, FString> GetLeftHandControllerToRecorderMapping(
        EFretDanceBasePosition Position, EFretDanceLeftHandState State) const;

    // 获取右手 Controller 到 Recorder 的完整映射（用于 SaveState）
    TMap<FString, FString> GetRightHandControllerToRecorderMapping(
        EFretDanceRightHandState State) const;

    // 获取左手 Recorder 到 Controller 的反向映射（用于 LoadState）
    TMap<FString, FString> GetLeftHandRecorderToControllerMapping(
        EFretDanceBasePosition Position, EFretDanceLeftHandState State) const;

    // 获取右手 Recorder 到 Controller 的反向映射（用于 LoadState）
    TMap<FString, FString> GetRightHandRecorderToControllerMapping(
        EFretDanceRightHandState State) const;

    // 检查左手状态组合是否有效
    bool IsValidLeftHandCombination(EFretDanceBasePosition Position,
                                    EFretDanceLeftHandState State) const;

    // 导入/导出
    void ExportRecorderInfo(const FString& FilePath);
    bool ImportRecorderInfo(const FString& FilePath);

    // 设置乐器类型（会自动更新配置）
    void SetInstrumentType(EFretDanceInstrumentType NewType);

    // 检查是否已初始化
    bool IsInitialized() const { return bIsInitialized; }

    // ControlRig 缓存访问方法
    UControlRig* GetCachedControlRig(FName ComponentName);
    UControlRigBlueprint* GetCachedControlRigBlueprint(FName ComponentName);

    // 注册所有 ControlRig 到缓存子系统（演奏者 + 吉他）
    void RegisterAllControlRigs();

    // 记录器初始化方法
    void InitializeRecorderTransforms();

    // 更新记录器键名（根据当前逻辑做 diff，只增删有变化的条目，不碰
    // RecorderTransforms）
    void UpdateRecorderKeys();

    // 当 ControlRig 相关操作失败时触发重新注册
    void TriggerControlRigReregistration(const FString& ErrorMessage);

   private:
    // ========== 右手记录器条目生成（单点定义拼接逻辑） ==========
    // 生成 {map_key, recorder_name} 键值对列表，供初始化和更新方法共用
    void GenerateRightHandRecorderEntries(
        TArray<TPair<FString, FString>>& OutEntries) const;

    // ========== 无效组合表 ==========
    UPROPERTY()
    TMap<EFretDanceBasePosition, FFretDanceInvalidLeftHandCombinations>
        InvalidCombinations;
};