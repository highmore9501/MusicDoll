#pragma once

#include "Animation/SkeletalMeshActor.h"
#include "ControlRig.h"
#include "CoreMinimal.h"
#include "ISequencer.h"
#include "LevelSequence.h"
#include "Sequencer/MovieSceneControlRigParameterTrack.h"
#include "AnimationBaker.generated.h"

/**
 * Control Rig扫描结果结构体
 * 存储单个Control Rig的扫描信息
 */
USTRUCT(BlueprintType)
struct FControlRigScanResult {
    GENERATED_BODY()

public:
    /** 绑定的SkeletalMeshActor */
    UPROPERTY(BlueprintReadOnly, Category = "Control Rig Scanner")
    ASkeletalMeshActor* BoundActor;

    /** Control Rig实例 */
    UPROPERTY(BlueprintReadOnly, Category = "Control Rig Scanner")
    UControlRig* ControlRigInstance;

    /** Control Rig蓝图 */
    UPROPERTY(BlueprintReadOnly, Category = "Control Rig Scanner")
    UControlRigBlueprint* ControlRigBlueprint;

    /** 可用的Control名称列表 */
    UPROPERTY(BlueprintReadOnly, Category = "Control Rig Scanner")
    TArray<FString> AvailableControls;

    /** 轨道名称 */
    UPROPERTY(BlueprintReadOnly, Category = "Control Rig Scanner")
    FString TrackName;

    FControlRigScanResult()
        : BoundActor(nullptr),
          ControlRigInstance(nullptr),
          ControlRigBlueprint(nullptr) {}

    // 析构函数 - 确保正确清理
    ~FControlRigScanResult() 
    {
        BoundActor = nullptr;
        ControlRigInstance = nullptr;
        ControlRigBlueprint = nullptr;
        AvailableControls.Empty();
        TrackName.Empty();
    }

    /** 检查结果是否有效 */
    bool IsValid() const {
        return BoundActor != nullptr && ControlRigInstance != nullptr &&
               ControlRigBlueprint != nullptr;
    }

    /** 获取Actor名称 */
    FString GetActorName() const {
        return BoundActor ? BoundActor->GetName() : TEXT("Unknown");
    }
};

// 声明进度回调委托
DECLARE_DYNAMIC_DELEGATE_TwoParams(FOnBakeProgress, int32, CurrentFrame, int32,
                                   TotalFrames);

/**
 * 烘焙设置结构体
 * 包含动画烘焙的各种参数配置
 */
USTRUCT(BlueprintType)
struct FAnimationBakeSettings {
    GENERATED_BODY()

   public:
    /** 开始帧 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Baking")
    int32 StartFrame;

    /** 结束帧 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Baking")
    int32 EndFrame;

    /** 帧步长 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Baking")
    int32 FrameStep;

    /** 是否覆盖现有关键帧 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Baking")
    bool bOverwriteExistingKeys;

    /** 烘焙精度（浮点数精度） */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Baking")
    float BakePrecision;

    FAnimationBakeSettings()
        : StartFrame(0),
          EndFrame(100),
          FrameStep(1),
          bOverwriteExistingKeys(true),
          BakePrecision(0.001f) {}

    /** 验证设置是否有效 */
    bool IsValid() const {
        return StartFrame >= 0 && EndFrame >= StartFrame && FrameStep > 0;
    }

    /** 获取总帧数 */
    int32 GetTotalFrames() const {
        return (EndFrame - StartFrame) / FrameStep + 1;
    }
};

/**
 * Control值快照结构体
 * 用于存储单个Control在特定帧的值
 */
USTRUCT(BlueprintType)
struct FControlValueSnapshot {
    GENERATED_BODY()

   public:
    /** 帧号 */
    UPROPERTY(BlueprintReadWrite, EditAnywhere)
    int32 FrameNumber;

    /** 位置 */
    UPROPERTY(BlueprintReadWrite, EditAnywhere)
    FVector Location;

    /** 旋转 */
    UPROPERTY(BlueprintReadWrite, EditAnywhere)
    FRotator Rotation;

    FControlValueSnapshot()
        : FrameNumber(0), Location(ForceInit), Rotation(ForceInit) {}

    FControlValueSnapshot(int32 InFrameNumber, const FVector& InLocation,
                          const FRotator& InRotation)
        : FrameNumber(InFrameNumber),
          Location(InLocation),
          Rotation(InRotation) {}
};

#if WITH_EDITOR

/**
 * 动画烘焙器
 * 负责Control Rig动画的烘焙核心逻辑
 */
UCLASS(BlueprintType)
class MUSICDOLLCOMMON_API UAnimationBaker : public UObject {
    GENERATED_BODY()

   public:
   

    /**
     * 多Control Rig Instance统一收集和分组写入
     * @param LevelSequence 目标LevelSequence
     * @param ControlGroups 控制组数据：{ControlRigInstance, {ControlName1, ControlName2, ...}}
     * @param Settings 烘焙设置
     * @param ProgressCallback 进度回调
     * @return 成功烘焙的Control数量
     */
    static int32 BakeMultiInstanceControlsUnified(
        ULevelSequence* LevelSequence,
        const TArray<TPair<UControlRig*, TArray<FString>>>& ControlGroups,
        const FAnimationBakeSettings& Settings,
        TFunction<void(int32, int32, const FString&)> ProgressCallback = nullptr);

    /**
     * 烘焙多个Control组（Actor级别接口）
     * @param LevelSequence 目标LevelSequence
     * @param ControlGroups 控制组数据：{ControlRigInstance, {ControlName1, ControlName2, ...}}
     * @param Settings 烘焙设置
     * @param ProgressCallback 进度回调
     * @return 成功烘焙的Control数量
     */
    static int32 BakeMultipleControlGroups(
        ULevelSequence* LevelSequence,
        const TArray<TPair<UControlRig*, TArray<FString>>>& ControlGroups,
        const FAnimationBakeSettings& Settings,
        TFunction<void(int32, int32, const FString&)> ProgressCallback = nullptr);
    

    /**
     * 基于Actor获取ControlRig信息的便捷方法
     * @param Actor 目标Actor
     * @param LevelSequence 当前LevelSequence
     * @param OutControlRig 输出：ControlRig实例
     * @param OutBlueprint 输出：ControlRig蓝图
     * @return 是否获取成功
     */
    static bool GetControlRigsForActor(ASkeletalMeshActor* Actor,
                                       ULevelSequence* LevelSequence,
                                       UControlRig*& OutControlRig,
                                       UControlRigBlueprint*& OutBlueprint);

    

   private:
    
    /**
     * 通过移动Sequencer播放光标来评估指定帧
     * 使用SetGlobalTime真正移动播放头，模拟人类拖动光标的操作
     * @param Sequencer Sequencer实例
     * @param LevelSequence Level序列
     * @param Frame 帧号（Display Rate帧号）
     * @return 是否评估成功
     */
    static bool EvaluateSequenceAtFrame(TSharedPtr<ISequencer> Sequencer, ULevelSequence* LevelSequence, int32 Frame);

    /**
     * 在帧评估后触发所有乐器Actor的同步逻辑
     * 遍历世界中所有AInstrumentBase子类，调用其同步方法
     * 这是烘焙时模拟Tick中同步行为的关键步骤
     */
    static void TriggerInstrumentSync();

    /**
     * 确保轨道Section存在
     * @param Track Control Rig轨道
     * @return Section指针
     */
    static UMovieSceneSection* EnsureTrackSection(
        UMovieSceneControlRigParameterTrack* Track);
};

#endif  // WITH_EDITOR