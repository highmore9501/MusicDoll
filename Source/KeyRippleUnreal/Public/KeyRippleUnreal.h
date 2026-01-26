#pragma once

#include "Animation/SkeletalMeshActor.h"
#include "ControlRig/Public/ControlRig.h"
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "InstrumentBase.h"  // 引入InstrumentBase头文件
#include "KeyRippleUnreal.generated.h"

UENUM(BlueprintType)
enum class EHandType : uint8 { LEFT = 0, RIGHT = 1 };

UENUM(BlueprintType)
enum class EKeyType : uint8 { WHITE = 0, BLACK = 1 };

UENUM(BlueprintType)
enum class EPositionType : uint8 { HIGH = 0, LOW = 1, MIDDLE = 2 };

USTRUCT(BlueprintType)
struct FSyncReport {
    GENERATED_BODY()

   public:
    UPROPERTY()
    bool bSuccess = true;

    UPROPERTY()
    TArray<FString> Warnings;

    UPROPERTY()
    TArray<FString> Errors;

    void AddWarning(const FString& Message) { Warnings.Add(Message); }

    void AddError(const FString& Message) {
        bSuccess = false;
        Errors.Add(Message);
    }

    void Clear() {
        bSuccess = true;
        Warnings.Empty();
        Errors.Empty();
    }
};

USTRUCT(BlueprintType)
struct FStringArray {
    GENERATED_BODY()

   public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Data")
    TArray<FString> Strings;

    FStringArray() {}

    void Add(const FString& String) { Strings.Add(String); }

    int32 Num() const { return Strings.Num(); }

    FString Get(int32 Index) const {
        if (Index >= 0 && Index < Strings.Num()) {
            return Strings[Index];
        }
        return FString();
    }

    void Clear() { Strings.Empty(); }
};

USTRUCT(BlueprintType)
struct FRecorderTransform {
    GENERATED_BODY()

   public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Transform")
    FVector Location;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Transform")
    FQuat Rotation;

    FRecorderTransform() : Location(ForceInit), Rotation(ForceInit) {}

    FRecorderTransform(const FVector& InLocation, const FQuat& InRotation)
        : Location(InLocation), Rotation(InRotation) {}

    FTransform ToTransform() const {
        return FTransform(Rotation, Location, FVector(1.0f));
    }

    void FromTransform(const FTransform& Transform) {
        Location = Transform.GetLocation();
        Rotation = Transform.GetRotation();
    }
};

// 定义控制关键帧数据结构
struct FControlKeyframe {
    int32 FrameNumber;
    FVector Translation;
    FQuat Rotation;  // 保持四元数形式直到最后插入时才转换

    // 辅助函数：将四元数转换为欧拉角（仅在需要时调用）
    FRotator GetEulerRotation() const {
        return Rotation.Rotator();
    }
};

UCLASS(Blueprintable, BlueprintType)
class KEYRIPPLEUNREAL_API AKeyRippleUnreal : public AInstrumentBase {  // 修改继承自InstrumentBase
    GENERATED_BODY()

   public:
    AKeyRippleUnreal();

   protected:
    virtual void BeginPlay() override;

   public:
    virtual void Tick(float DeltaTime) override;

    UPROPERTY(EditAnywhere, BlueprintReadWrite,
              Category = "KeyRipple Configuration | SkeletalMesh")
    ASkeletalMeshActor* Piano;

    UPROPERTY(EditAnywhere, BlueprintReadWrite,
              Category = "KeyRipple Configuration | SkeletalMesh")
    class UMaterialInstance* KeyMatWhite;

    UPROPERTY(EditAnywhere, BlueprintReadWrite,
              Category = "KeyRipple Configuration | SkeletalMesh")
    class UMaterialInstance* KeyMatBlack;

    UPROPERTY(EditAnywhere, BlueprintReadWrite,
              Category = "KeyRipple Configuration")
    int32 OneHandFingerNumber;

    UPROPERTY(EditAnywhere, BlueprintReadWrite,
              Category = "KeyRipple Configuration")
    int32 LeftestPosition;

    UPROPERTY(EditAnywhere, BlueprintReadWrite,
              Category = "KeyRipple Configuration")
    int32 LeftPosition;

    UPROPERTY(EditAnywhere, BlueprintReadWrite,
              Category = "KeyRipple Configuration")
    int32 MiddleLeftPosition;

    UPROPERTY(EditAnywhere, BlueprintReadWrite,
              Category = "KeyRipple Configuration")
    int32 MiddleRightPosition;

    UPROPERTY(EditAnywhere, BlueprintReadWrite,
              Category = "KeyRipple Configuration")
    int32 RightPosition;

    UPROPERTY(EditAnywhere, BlueprintReadWrite,
              Category = "KeyRipple Configuration")
    int32 RightestPosition;

    UPROPERTY(EditAnywhere, BlueprintReadWrite,
              Category = "KeyRipple Configuration")
    int32 MinKey;

    UPROPERTY(EditAnywhere, BlueprintReadWrite,
              Category = "KeyRipple Configuration")
    int32 MaxKey;

    UPROPERTY(EditAnywhere, BlueprintReadWrite,
              Category = "KeyRipple Configuration")
    int32 HandRange;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "KeyRipple State")
    EKeyType LeftHandKeyType;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "KeyRipple State")
    EPositionType LeftHandPositionType;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "KeyRipple State")
    EKeyType RightHandKeyType;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "KeyRipple State")
    EPositionType RightHandPositionType;

    // 注意：SkeletalMeshActor、IOFilePath 和 AnimationFilePath 现在从基类继承，不需要在这里重复声明
    // FString KeyRippleFilePath 已重命名为 AnimationFilePath 并移至基类

    UPROPERTY()
    TMap<FString, FString> FingerControllers;

    UPROPERTY()
    TMap<FString, FStringArray> FingerRecorders;

    UPROPERTY()
    TMap<FString, FString> HandControllers;

    UPROPERTY()
    TMap<FString, FStringArray> HandRecorders;

    UPROPERTY()
    TMap<FString, FString> KeyBoardPositions;

    UPROPERTY()
    TMap<FString, FString> Guidelines;

    UPROPERTY()
    TMap<FString, FString> TargetPoints;

    UPROPERTY()
    TMap<FString, FStringArray> TargetPointsRecorders;

    UPROPERTY()
    TMap<FString, FString> ShoulderControllers;

    UPROPERTY()
    TMap<FString, FStringArray> ShoulderRecorders;

    UPROPERTY()
    TMap<FString, FString> PolePoints;

    FString GetControllerName(int32 FingerNumber, EHandType HandType) const;
    FString GetRecorderName(EPositionType PositionType, EKeyType KeyType,
                            int32 FingerNumber, EHandType HandType) const;
    FString GetHandControllerName(const FString& HandControllerType,
                                  EHandType HandType) const;
    FString GetHandRecorderName(EPositionType PositionType, EKeyType KeyType,
                                const FString& HandControllerType,
                                EHandType HandType) const;

    void InitializeControllersAndRecorders();

    FString GetPositionTypeString(EPositionType PositionType) const;

    FString GetKeyTypeString(EKeyType KeyType) const;

    UPROPERTY()
    TMap<FString, class AActor*> CreatedActors;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "KeyRipple Data")
    TMap<FString, FRecorderTransform> RecorderTransforms;

    UPROPERTY()
    TMap<FString, class UMaterialInstanceConstant*> GeneratedPianoMaterials;
};