#pragma once

#include "Animation/SkeletalMeshActor.h"
#include "ControlRig/Public/ControlRig.h"
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "InstrumentBase.h" 
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
    /** 字符串数组 */
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
    /** 位置 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Transform")
    FVector Location;

    /** 旋转 */
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
    FQuat Rotation; 

    // 辅助函数：将四元数转换为欧拉角
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

    /** 钢琴模型 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite,
              Category = "Basic Properties")
    ASkeletalMeshActor* Piano;

    /** 白键材质 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite,
              Category = "Basic Properties")
    class UMaterialInstance* KeyMatWhite;

    /** 黑键材质 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite,
              Category = "Basic Properties")
    class UMaterialInstance* KeyMatBlack;

    /** 单手手指数量 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite,
              Category = "KeyRipple Configuration")
    int32 OneHandFingerNumber;

    /** 最左侧位置 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite,
              Category = "KeyRipple Configuration")
    int32 LeftestPosition;

    /** 左侧位置 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite,
              Category = "KeyRipple Configuration")
    int32 LeftPosition;

    /** 中左位置 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite,
              Category = "KeyRipple Configuration")
    int32 MiddleLeftPosition;

    /** 中右位置 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite,
              Category = "KeyRipple Configuration")
    int32 MiddleRightPosition;

    /** 右侧位置 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite,
              Category = "KeyRipple Configuration")
    int32 RightPosition;

    /** 最右侧位置 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite,
              Category = "KeyRipple Configuration")
    int32 RightestPosition;

    /** 最小音键 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite,
              Category = "KeyRipple Configuration")
    int32 MinKey;

    /** 最大音键 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite,
              Category = "KeyRipple Configuration")
    int32 MaxKey;

    /** 单手跨度 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite,
              Category = "KeyRipple Configuration")
    int32 HandRange;

    /** 左手按键类型 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "KeyRipple State")
    EKeyType LeftHandKeyType;

    /** 左手位置类型 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "KeyRipple State")
    EPositionType LeftHandPositionType;

    /** 右手按键类型 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "KeyRipple State")
    EKeyType RightHandKeyType;

    /** 右手位置类型 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "KeyRipple State")
    EPositionType RightHandPositionType;    

    /** 手指控制器 */
    UPROPERTY()
    TMap<FString, FString> FingerControllers;

    /** 手指记录器 */
    UPROPERTY()
    TMap<FString, FStringArray> FingerRecorders;

    /** 手部控制器 */
    UPROPERTY()
    TMap<FString, FString> HandControllers;

    /** 手部记录器 */
    UPROPERTY()
    TMap<FString, FStringArray> HandRecorders;

    /** 键盘关键位置点 */
    UPROPERTY()
    TMap<FString, FString> KeyBoardPositions;

    /** 方向线 */
    UPROPERTY()
    TMap<FString, FString> Guidelines;

    /** 人物朝向控制器 */
    UPROPERTY()
    TMap<FString, FString> TargetPoints;

    /** 人物朝向记录器 */
    UPROPERTY()
    TMap<FString, FStringArray> TargetPointsRecorders;

    /** 肩部控制器 */
    UPROPERTY()
    TMap<FString, FString> ShoulderControllers;

    /** 肩部记录器 */
    UPROPERTY()
    TMap<FString, FStringArray> ShoulderRecorders;

    /** 手指极向量 */
    UPROPERTY()   
    TMap<FString, FString> PolePoints;

    /** 右手原始方向 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "KeyRipple Configuration")
    FVector RightHandOriginalDirection;

    /** 左手原始方向 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "KeyRipple Configuration")
    FVector LeftHandOriginalDirection;

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

    /** 已创建的Actor对象 */
    UPROPERTY()
    TMap<FString, class AActor*> CreatedActors;

    /** 记录器变换数据 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "KeyRipple Data")    
    TMap<FString, FRecorderTransform> RecorderTransforms;

    /** 生成的钢琴材质 */
    UPROPERTY()
    TMap<FString, class UMaterialInstanceConstant*> GeneratedPianoMaterials;
};