#pragma once

#include "CoreMinimal.h"
#include "KeyRippleAnimationProcessor.h"  // 包含动画处理函数
#include "Animation/SkeletalMeshActor.h"  // 包含SkeletalMeshActor定义
#include "ControlRig.h"
#include "ControlRigBlueprintLegacy.h"
#include "ControlRigSequencerEditorLibrary.h"
#include "KeyRippleUnreal.h"  // 引入原类的定义
#include "LevelSequence.h"
#include "MovieScene.h"
#include "KeyRippleControlRigProcessor.generated.h"

// 前置声明
class UControlRig;
class AActor;

class UMovieSceneTrack;
class UMovieSceneSection;

/**
 * Control Rig 处理器类，用于处理与 Control Rig 相关的操作
 */
UCLASS()

class KEYRIPPLEUNREAL_API UKeyRippleControlRigProcessor : public UObject {
    GENERATED_BODY()

   public:
    /**
     * 执行对象状态检查
     * @param KeyRippleActor KeyRippleUnreal 实例
     */
    UFUNCTION(BlueprintCallable, Category = "KeyRipple Control Rig Processor")
    static void CheckObjectsStatus(AKeyRippleUnreal* KeyRippleActor);

    /**
     * 设置所有对象
     * @param KeyRippleActor KeyRippleUnreal 实例
     */
    UFUNCTION(BlueprintCallable, Category = "KeyRipple Control Rig Processor")
    static void SetupAllObjects(AKeyRippleUnreal* KeyRippleActor);

    /**
     * 保存状态
     * @param KeyRippleActor KeyRippleUnreal 实例
     */
    UFUNCTION(BlueprintCallable, Category = "KeyRipple Control Rig Processor")
    static void SaveState(AKeyRippleUnreal* KeyRippleActor);

    /**
     * 加载状态
     * @param KeyRippleActor KeyRippleUnreal 实例
     */
    UFUNCTION(BlueprintCallable, Category = "KeyRipple Control Rig Processor")
    static void LoadState(AKeyRippleUnreal* KeyRippleActor);

    /**
     * 导出记录器信息
     * @param KeyRippleActor KeyRippleUnreal 实例
     */
    UFUNCTION(BlueprintCallable, Category = "KeyRipple Control Rig Processor")
    static void ExportRecorderInfo(AKeyRippleUnreal* KeyRippleActor);

    /**
     * 导入记录器信息
     * @param KeyRippleActor KeyRippleUnreal 实例
     * @return 是否成功导入
     */
    UFUNCTION(BlueprintCallable, Category = "KeyRipple Control Rig Processor")
    static bool ImportRecorderInfo(AKeyRippleUnreal* KeyRippleActor);

    /**
     * 从Recorder名字中提取Control名字
     * @param RecorderName 录制器名字
     * @return 对应的Control名字
     */
    static FString GetControlNameFromRecorder(const FString& RecorderName);

    /**
     * 创建控制器
     * @param KeyRippleActor KeyRippleUnreal 实例
     * @param ControllerName 控制器名称
     * @param bIsRotation 是否为旋转控制器
     * @return 创建的Actor实例
     */
    UFUNCTION(BlueprintCallable, Category = "KeyRipple Control Rig Processor")
    static AActor* CreateController(AKeyRippleUnreal* KeyRippleActor,
                                    const FString& ControllerName,
                                    bool bIsRotation,
                                    const FString& ParentControllerName = "");

    /**
     * 设置目标Actor驱动
     * @param KeyRippleActor KeyRippleUnreal 实例
     * @param TargetActor 目标Actor
     */
    UFUNCTION(BlueprintCallable, Category = "KeyRipple Control Rig Processor")
    static void SetupTargetActorDriver(AKeyRippleUnreal* KeyRippleActor,
                                       AActor* TargetActor);

    /**
     * 清理未使用的Actor
     * @param KeyRippleActor KeyRippleUnreal 实例
     */
    UFUNCTION(BlueprintCallable, Category = "KeyRipple Control Rig Processor")
    static void CleanupUnusedActors(AKeyRippleUnreal* KeyRippleActor);

    /**
     * 设置控制器
     * @param KeyRippleActor KeyRippleUnreal 实例
     */
    UFUNCTION(BlueprintCallable, Category = "KeyRipple Control Rig Processor")
    static void SetupControllers(AKeyRippleUnreal* KeyRippleActor);

    /**
     * 设置记录器
     * @param KeyRippleActor KeyRippleUnreal 实例
     */
    UFUNCTION(BlueprintCallable, Category = "KeyRipple Control Rig Processor")
    static void SetupRecorders(AKeyRippleUnreal* KeyRippleActor);

    /**
     * 通过SkeletalMeshActor获取Control Rig Instance和Blueprint
     * @param SkeletalMeshActor Skeletal Mesh Actor实例
     * @param OutControlRigInstance 输出参数，Control Rig实例
     * @param OutControlRigBlueprint 输出参数，Control Rig蓝图
     * @return 是否成功获取
     */
    UFUNCTION(BlueprintCallable, Category = "KeyRipple Control Rig Processor")
    static bool GetControlRigFromSkeletalMeshActor(
        ASkeletalMeshActor* SkeletalMeshActor,
        UControlRig*& OutControlRigInstance,
        UControlRigBlueprint*& OutControlRigBlueprint);

    /**
     * 根据Control名字和当前手部状态获取对应的Recorder名字
     * @param KeyRippleActor KeyRippleUnreal实例
     * @param ControlName Control的名字
     * @param bIsFingerControl 是否为手指控制器
     * @return Recorder的名字
     */
    static FString GetRecorderNameForControl(AKeyRippleUnreal* KeyRippleActor,
                                             const FString& ControlName,
                                             bool bIsFingerControl);
};
