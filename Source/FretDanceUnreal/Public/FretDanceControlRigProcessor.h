#pragma once

#include "ControlRigCreationUtility.h"
#include "CoreMinimal.h"
#include "FretDanceUnreal.h"
#include "UObject/Object.h"
#include "ControlRigBlueprintLegacy.h"
#include "FretDanceControlRigProcessor.generated.h"

/**
 * FretDance Control Rig处理器
 * 负责创建和管理吉他的Control Rig控制器
 *
 * 与StringFlow的主要区别：
 * 1. 控制器结构更简单：基于基础位置(P0-P4)和手部状态，而非复杂的弦/品格系统
 * 2. 乐器类型差异化：支持指弹吉他/电吉他/贝斯的不同控制层级
 * 3. 命名规范简化：采用{Position}_{State}_{Controller}格式
 * 4. 验证逻辑简化：基于无效组合表而非复杂的数学计算
 */
UCLASS()
class FRETDANCEUNREAL_API UFretDanceControlRigProcessor : public UObject {
    GENERATED_BODY()

   public:
    /**
     * 在Control Rig Hierarchy中创建所有必要的控制器
     *
     * @param FretDanceActor FretDance实例
     * @return 成功创建的控制器数量
     *
     * 与StringFlow的区别：
     * - 不需要复杂的弦/品格控制器创建逻辑
     * - 根据乐器类型调整右手控制器层级结构
     * - 使用更简单的命名规范
     */
    static int32 SetupControllers(AFretDanceUnreal* FretDanceActor);

    /**
     * 验证所有预期的控制器是否都已创建
     *
     * @param FretDanceActor FretDance实例
     * @return 验证是否通过
     *
     * 与StringFlow的区别：
     * - 验证逻辑更简单，基于预定义的控制器列表
     * - 不需要复杂的正则表达式匹配
     */
    static bool CheckObjectsStatus(AFretDanceUnreal* FretDanceActor);

    /**
     * 完整的初始化流程：创建控制器 + 验证状态
     *
     * @param FretDanceActor FretDance实例
     * @return 初始化是否成功
     */
    static bool SetupAllObjects(AFretDanceUnreal* FretDanceActor);

    /**
     * 保存当前控制器状态（总入口）- 保存到 RecorderTransforms
     *
     * @param FretDanceActor FretDance 实例
     * @param OutStateData 输出的状态数据（保留参数以兼容调用接口）
     * @return 是否保存成功
     */
    static bool SaveState(AFretDanceUnreal* FretDanceActor,
                          TMap<FString, FTransform>& OutStateData);

    /**
     * 保存左手状态到 RecorderTransforms
     *
     * @param FretDanceActor FretDance 实例
     * @return 是否保存成功
     */
    static bool SaveLeftHandState(AFretDanceUnreal* FretDanceActor);

    /**
     * 保存右手状态到 RecorderTransforms
     *
     * @param FretDanceActor FretDance 实例
     * @return 是否保存成功
     */
    static bool SaveRightHandState(AFretDanceUnreal* FretDanceActor);

    /**
     * 从 RecorderTransforms 加载控制器状态（同时加载左右手）
     *
     * @param FretDanceActor FretDance 实例
     * @param StateData 要加载的状态数据（保留参数以兼容调用接口）
     * @return 是否加载成功
     */
    static bool LoadState(AFretDanceUnreal* FretDanceActor,
                          const TMap<FString, FTransform>& StateData);

    /**
     * 保存辅助线状态到 RecorderTransforms
     *
     * @param FretDanceActor FretDance 实例
     * @return 是否保存成功
     */
    static bool SaveGuidelinesState(AFretDanceUnreal* FretDanceActor);

    /**
     * 从 RecorderTransforms 加载辅助线状态
     *
     * @param FretDanceActor FretDance 实例
     * @return 是否加载成功
     */
    static bool LoadGuidelinesState(AFretDanceUnreal* FretDanceActor);

    /**
     * 保存指板位置状态到 RecorderTransforms（与左手状态无关）
     *
     * @param FretDanceActor FretDance 实例
     * @return 是否保存成功
     */
    static bool SaveFretPositionsState(AFretDanceUnreal* FretDanceActor);

    /**
     * 从 RecorderTransforms 加载指板位置状态（与左手状态无关）
     *
     * @param FretDanceActor FretDance 实例
     * @return 是否加载成功
     */
    static bool LoadFretPositionsState(AFretDanceUnreal* FretDanceActor);

   private:
    /**
     * 验证FretDance Actor的基本状态
     */
    static bool ValidateFretDanceActor(AFretDanceUnreal* FretDanceActor,
                                       const FString& FunctionName);

    /**
     * 获取Control Rig实例
     */
    static UControlRig* GetControlRig(AFretDanceUnreal* FretDanceActor);

    /**
     * 创建单个控制器
     */
    static bool CreateController(
        UControlRigBlueprint* ControlRigBlueprint,
        const FString& ControllerName, const FString& ParentName = FString(),
        const FTransform& Transform = FTransform::Identity);

    /**
     * 获取预期的控制器名称列表
     */
    static TArray<FString> GetExpectedControllerNames(
        AFretDanceUnreal* FretDanceActor);

    /**
     * 根据乐器类型获取右手控制器层级结构
     */
    static TMap<FString, FString> GetRightHandControllerHierarchy(
        EFretDanceInstrumentType InstrumentType, FString ControllerRootName);
};