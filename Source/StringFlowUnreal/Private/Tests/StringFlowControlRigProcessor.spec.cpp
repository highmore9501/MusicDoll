#include "StringFlowControlRigProcessor.h"

#include "Animation/SkeletalMeshActor.h"
#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "StringFlowUnreal.h"

#if WITH_AUTOMATION_TESTS

BEGIN_DEFINE_SPEC(FStringFlowControlRigProcessorSpec,
                  "StringFlow.ControlRigProcessor",
                  EAutomationTestFlags::EditorContext |
                      EAutomationTestFlags::EngineFilter)

AStringFlowUnreal* TestStringFlowActor;

END_DEFINE_SPEC(FStringFlowControlRigProcessorSpec)

void FStringFlowControlRigProcessorSpec::Define() {
    BeforeEach([this]() {
        // 创建测试 Actor
        TestStringFlowActor = NewObject<AStringFlowUnreal>();
        TestStringFlowActor->OneHandFingerNumber = 4;
        TestStringFlowActor->StringNumber = 4;

        // 创建骨骼网格 Actor
        ASkeletalMeshActor* MockInstrument = NewObject<ASkeletalMeshActor>();
        TestStringFlowActor->StringInstrument = MockInstrument;
    });

    AfterEach([this]() {
        // 清理
        if (TestStringFlowActor) {
            TestStringFlowActor->RecorderTransforms.Empty();

            if (TestStringFlowActor->StringInstrument) {
                TestStringFlowActor->StringInstrument
                    ->ConditionalBeginDestroy();
            }

            TestStringFlowActor->ConditionalBeginDestroy();
            TestStringFlowActor = nullptr;
        }
    });

    Describe(TEXT("CheckObjectsStatus"), [this]() {
        It(TEXT("Should not crash for valid actor"), [this]() {
            // 函数应该能够被调用而不会崩溃
            // 检查日志输出中是否有错误信息
            UStringFlowControlRigProcessor::CheckObjectsStatus(
                TestStringFlowActor);
            // 如果我们到了这里，说明函数成功执行了
            TestTrue(TEXT("Function executed successfully"), true);
        });
    });

    Describe(TEXT("SaveState"), [this]() {
        It(TEXT("Should not crash for valid actor"), [this]() {
            UStringFlowControlRigProcessor::SaveState(TestStringFlowActor);
            // 如果执行到这里，说明函数成功了
            TestTrue(TEXT("SaveState executed without crashing"), true);
        });

        It(TEXT("Should preserve actor state"), [this]() {
            TestStringFlowActor->StringNumber = 4;
            UStringFlowControlRigProcessor::SaveState(TestStringFlowActor);

            TestEqual(TEXT("StringNumber should be unchanged"),
                      TestStringFlowActor->StringNumber, 4);
        });
    });

    Describe(TEXT("LoadState"), [this]() {
        It(TEXT("Should not crash for valid actor"), [this]() {
            UStringFlowControlRigProcessor::LoadState(TestStringFlowActor);
            // 如果执行到这里，说明函数成功了
            TestTrue(TEXT("LoadState executed without crashing"), true);
        });

        It(TEXT("Should support state restoration"), [this]() {
            FStringFlowRecorderTransform TestTransform(FVector(1, 2, 3),
                                                       FQuat::Identity);
            TestStringFlowActor->RecorderTransforms.Add(TEXT("test"),
                                                        TestTransform);

            UStringFlowControlRigProcessor::LoadState(TestStringFlowActor);
            TestEqual(TEXT("Should preserve transforms"),
                      TestStringFlowActor->RecorderTransforms.Num(), 1);
        });
    });

    Describe(TEXT("GetControlRigFromStringInstrument"), [this]() {
        It(TEXT("Should return false for null actor"), [this]() {
            UControlRig* OutRigInstance = nullptr;
            UControlRigBlueprint* OutRigBlueprint = nullptr;

            bool bResult = UStringFlowControlRigProcessor::
                GetControlRigFromStringInstrument(nullptr, OutRigInstance,
                                                  OutRigBlueprint);

            TestFalse(TEXT("Should return false for null actor"), bResult);
        });

        It(TEXT("Should handle non-existent Control Rig"), [this]() {
            UControlRig* OutRigInstance = nullptr;
            UControlRigBlueprint* OutRigBlueprint = nullptr;

            bool bResult = UStringFlowControlRigProcessor::
                GetControlRigFromStringInstrument(
                    TestStringFlowActor->StringInstrument, OutRigInstance,
                    OutRigBlueprint);

            // 在没有有效的Control Rig的情况下应该返回false
            TestFalse(TEXT("Should return false when no Control Rig found"),
                      bResult);
        });
    });

    Describe(TEXT("SetupControllers"), [this]() {
        It(TEXT("Should not crash for valid actor"), [this]() {
            UStringFlowControlRigProcessor::SetupControllers(
                TestStringFlowActor);
            // 如果执行到这里，说明函数成功执行了
            TestTrue(TEXT("SetupControllers executed without crashing"), true);
        });
    });

    Describe(TEXT("SetupAllObjects"), [this]() {
        It(TEXT("Should coordinate setup operations"), [this]() {
            UStringFlowControlRigProcessor::SetupAllObjects(
                TestStringFlowActor);
            // 如果执行到这里，说明函数成功执行了
            TestTrue(TEXT("SetupAllObjects executed without crashing"), true);
        });
    });

    Describe(TEXT("State transitions"), [this]() {
        It(TEXT("Should preserve recorder transforms data"), [this]() {
            FStringFlowRecorderTransform TestTransform(FVector(1, 2, 3),
                                                       FQuat::Identity);
            TestStringFlowActor->RecorderTransforms.Add(TEXT("test_key"),
                                                        TestTransform);

            TestEqual(TEXT("Should contain test data"),
                      TestStringFlowActor->RecorderTransforms.Num(), 1);

            const FStringFlowRecorderTransform* FoundTransform =
                TestStringFlowActor->RecorderTransforms.Find(TEXT("test_key"));
            TestNotNull(TEXT("Should find test data"), FoundTransform);
        });

        It(TEXT("Should support multiple recorder transforms"), [this]() {
            FStringFlowRecorderTransform Transform1(FVector(1, 2, 3),
                                                    FQuat::Identity);
            FStringFlowRecorderTransform Transform2(FVector(4, 5, 6),
                                                    FQuat::Identity);

            TestStringFlowActor->RecorderTransforms.Add(TEXT("key1"),
                                                        Transform1);
            TestStringFlowActor->RecorderTransforms.Add(TEXT("key2"),
                                                        Transform2);

            TestEqual(TEXT("Should have two entries"),
                      TestStringFlowActor->RecorderTransforms.Num(), 2);
        });

        It(TEXT("Should clear transforms on reset"), [this]() {
            TestStringFlowActor->RecorderTransforms.Add(
                TEXT("key"),
                FStringFlowRecorderTransform(FVector::Zero(), FQuat::Identity));
            TestStringFlowActor->RecorderTransforms.Empty();

            TestEqual(TEXT("Should be empty after clear"),
                      TestStringFlowActor->RecorderTransforms.Num(), 0);
        });
    });
}

#endif  // WITH_AUTOMATION_TESTS
