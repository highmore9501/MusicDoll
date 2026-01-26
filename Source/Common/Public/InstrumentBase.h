#pragma once

#include "Animation/SkeletalMeshActor.h"
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "InstrumentBase.generated.h"

/**
 * 乐器通用基类，用于统一处理各种乐器相关的功能
 */
UCLASS(Abstract, Blueprintable)
class COMMON_API AInstrumentBase : public AActor
{
    GENERATED_BODY()

public:
    AInstrumentBase();

protected:
    virtual void BeginPlay() override;

public:
    virtual void Tick(float DeltaTime) override;

    /** 要操作的骨骼网格体Actor */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Instrument Configuration")
    ASkeletalMeshActor* SkeletalMeshActor;

    /** 输入输出文件路径 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Instrument Configuration")
    FString IOFilePath;

    /** 动画文件路径 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Instrument Configuration")
    FString AnimationFilePath;
};