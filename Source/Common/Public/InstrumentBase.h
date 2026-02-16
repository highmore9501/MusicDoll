#pragma once

#include "Animation/SkeletalMeshActor.h"
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "InstrumentBase.generated.h"

// 前置声明
class FInstrumentControlRigUtility;

/**
 * 乐器通用基类，用于统一处理各种乐器相关的功能
 */
UCLASS(Abstract, Blueprintable)
class COMMON_API AInstrumentBase : public AActor {
    GENERATED_BODY()

   public:
    AInstrumentBase();

   protected:
    virtual void BeginPlay() override;

   public:
    virtual void Tick(float DeltaTime) override;

    /** 演奏者的骨骼 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Basic Properties")
    ASkeletalMeshActor* SkeletalMeshActor;

    /** 设置文件路径 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "IO Configuration")
    FString IOFilePath;

    /** 动画文件路径 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "IO Configuration")
    FString AnimationFilePath;
};