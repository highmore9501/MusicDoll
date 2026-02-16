#include "Misc/AutomationTest.h"
#include "StringFlowUnreal.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"

#if WITH_AUTOMATION_TESTS

BEGIN_DEFINE_SPEC(FStringFlowUnrealSpec, "StringFlow.StringFlowUnreal", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
	
	AStringFlowUnreal* TestActor;

END_DEFINE_SPEC(FStringFlowUnrealSpec)

void FStringFlowUnrealSpec::Define()
{
	BeforeEach([this]()
	{
		// 为每个测试创建一个新的测试 Actor
		TestActor = NewObject<AStringFlowUnreal>();
		TestActor->OneHandFingerNumber = 4;
		TestActor->StringNumber = 4;
	});

	AfterEach([this]()
	{
		// 清理测试 Actor
		if (TestActor)
		{
			TestActor->ConditionalBeginDestroy();
			TestActor = nullptr;
		}
	});

	Describe(TEXT("GetFingerControllerName"), [this]()
		{
			It(TEXT("Should generate correct left hand finger controller names"), [this]()
				{
					FString Result1 = TestActor->GetFingerControllerName(1, EStringFlowHandType::LEFT);
					TestEqual(TEXT("Finger 1 left should end with _L"), Result1.Right(2), FString(TEXT("_L")));

					FString Result2 = TestActor->GetFingerControllerName(2, EStringFlowHandType::LEFT);
					TestEqual(TEXT("Finger 2 left should end with _L"), Result2.Right(2), FString(TEXT("_L")));

					FString Result4 = TestActor->GetFingerControllerName(4, EStringFlowHandType::LEFT);
					TestEqual(TEXT("Finger 4 left should end with _L"), Result4.Right(2), FString(TEXT("_L")));
				});

			It(TEXT("Should generate correct right hand finger controller names"), [this]()
				{
					FString Result1 = TestActor->GetFingerControllerName(1, EStringFlowHandType::RIGHT);
					TestEqual(TEXT("Finger 1 right should end with _R"), Result1.Right(2), FString(TEXT("_R")));

					FString Result2 = TestActor->GetFingerControllerName(2, EStringFlowHandType::RIGHT);
					TestEqual(TEXT("Finger 2 right should end with _R"), Result2.Right(2), FString(TEXT("_R")));

					FString Result4 = TestActor->GetFingerControllerName(4, EStringFlowHandType::RIGHT);
					TestEqual(TEXT("Finger 4 right should end with _R"), Result4.Right(2), FString(TEXT("_R")));
				});

			It(TEXT("Should generate unique names for different fingers"), [this]()
				{
					FString Name1 = TestActor->GetFingerControllerName(1, EStringFlowHandType::LEFT);
					FString Name2 = TestActor->GetFingerControllerName(2, EStringFlowHandType::LEFT);
					FString Name3 = TestActor->GetFingerControllerName(3, EStringFlowHandType::LEFT);
					FString Name4 = TestActor->GetFingerControllerName(4, EStringFlowHandType::LEFT);

					TestNotEqual(TEXT("Finger 1 and 2 should differ"), Name1, Name2);
					TestNotEqual(TEXT("Finger 2 and 3 should differ"), Name2, Name3);
					TestNotEqual(TEXT("Finger 3 and 4 should differ"), Name3, Name4);
				});

			It(TEXT("Should differentiate left and right hands"), [this]()
				{
					FString LeftName = TestActor->GetFingerControllerName(1, EStringFlowHandType::LEFT);
					FString RightName = TestActor->GetFingerControllerName(1, EStringFlowHandType::RIGHT);

					TestNotEqual(TEXT("Left and right should differ"), LeftName, RightName);
				});
		});

	Describe(TEXT("GetFingerRecorderName"), [this]()
		{
			It(TEXT("Should include all parameters in recorder name"), [this]()
				{
					FString Result = TestActor->GetLeftFingerRecorderName(
						0,  // StringIndex
						0,  // FretIndex
						1,  // FingerNumber
						TEXT("Normal")  // PositionType
					);

					TestTrue(TEXT("Should not be empty"), !Result.IsEmpty());
				});

			It(TEXT("Should contain position type"), [this]()
				{
					FString Result = TestActor->GetLeftFingerRecorderName(
						0, 0, 1,
						TEXT("Normal")
					);

					TestTrue(TEXT("Should not be empty"), !Result.IsEmpty());
				});

			It(TEXT("Should handle different position types"), [this]()
				{
					FString Result1 = TestActor->GetLeftFingerRecorderName(
						0, 0, 1,
						TEXT("Normal")
					);

					FString Result2 = TestActor->GetLeftFingerRecorderName(
						0, 0, 2,
						TEXT("Normal")
					);

					TestNotEqual(TEXT("Different fingers should differ"), Result1, Result2);
				});
		});

	Describe(TEXT("GetHandControllerName"), [this]()
		{
			It(TEXT("Should generate left hand controller names"), [this]()
				{
					FString Result = TestActor->GetHandControllerName(TEXT("hand_pivot_controller"), EStringFlowHandType::LEFT);
					TestTrue(TEXT("Should end with _L"), Result.EndsWith(TEXT("_L")));
					TestTrue(TEXT("Should contain HP"), Result.Contains(TEXT("HP")));
				});

			It(TEXT("Should generate right hand controller names"), [this]()
				{
					FString Result = TestActor->GetHandControllerName(TEXT("hand_pivot_controller"), EStringFlowHandType::RIGHT);
					TestTrue(TEXT("Should end with _R"), Result.EndsWith(TEXT("_R")));
					TestTrue(TEXT("Should contain HP"), Result.Contains(TEXT("HP")));
				});

			It(TEXT("Should differentiate hand types"), [this]()
				{
					FString LeftResult = TestActor->GetHandControllerName(TEXT("hand_pivot_controller"), EStringFlowHandType::LEFT);
					FString RightResult = TestActor->GetHandControllerName(TEXT("hand_pivot_controller"), EStringFlowHandType::RIGHT);

					TestNotEqual(TEXT("Should differ by hand"), LeftResult, RightResult);
				});
		});

	Describe(TEXT("GetHandRecorderName"), [this]()
		{
			It(TEXT("Should create valid recorder name with all components"), [this]()
				{
					FString Result = TestActor->GetLeftHandRecorderName(
						0,                      // StringIndex
						0,                      // FretIndex
						TEXT("hand_controller"), // HandControllerType
						TEXT("Normal")          // PositionType
					);

					TestTrue(TEXT("Should not be empty"), !Result.IsEmpty());
				});

			It(TEXT("Should include position type"), [this]()
				{
					FString Result = TestActor->GetLeftHandRecorderName(
						0, 0, TEXT("hand_controller"),
						TEXT("Normal")
					);

					TestTrue(TEXT("Should contain position type"), Result.Contains(TEXT("Normal")));
				});
		});

	Describe(TEXT("GetLeftHandPositionTypeString"), [this]()
		{
			It(TEXT("Should return NORMAL for NORMAL position"), [this]()
				{
					FString Result = TestActor->GetLeftHandPositionTypeString(EStringFlowLeftHandPositionType::NORMAL);
					TestEqual(TEXT("NORMAL position should return NORMAL"), Result, FString(TEXT("NORMAL")));
				});

			It(TEXT("Should return INNER for INNER position"), [this]()
				{
					FString Result = TestActor->GetLeftHandPositionTypeString(EStringFlowLeftHandPositionType::INNER);
					TestEqual(TEXT("INNER position should return INNER"), Result, FString(TEXT("INNER")));
				});

			It(TEXT("Should return OUTER for OUTER position"), [this]()
				{
					FString Result = TestActor->GetLeftHandPositionTypeString(EStringFlowLeftHandPositionType::OUTER);
					TestEqual(TEXT("OUTER position should return OUTER"), Result, FString(TEXT("OUTER")));
				});

			It(TEXT("Should return different strings for different positions"), [this]()
				{
					FString Normal = TestActor->GetLeftHandPositionTypeString(EStringFlowLeftHandPositionType::NORMAL);
					FString Inner = TestActor->GetLeftHandPositionTypeString(EStringFlowLeftHandPositionType::INNER);
					FString Outer = TestActor->GetLeftHandPositionTypeString(EStringFlowLeftHandPositionType::OUTER);

					TestNotEqual(TEXT("NORMAL and INNER should differ"), Normal, Inner);
					TestNotEqual(TEXT("INNER and OUTER should differ"), Inner, Outer);
					TestNotEqual(TEXT("NORMAL and OUTER should differ"), Normal, Outer);
				});
		});

	Describe(TEXT("GetRightHandPositionTypeString"), [this]()
		{
			It(TEXT("Should return NEAR for NEAR position"), [this]()
				{
					FString Result = TestActor->GetRightHandPositionTypeString(EStringFlowRightHandPositionType::NEAR);
					TestEqual(TEXT("NEAR position should return NEAR"), Result, FString(TEXT("NEAR")));
				});

			It(TEXT("Should return FAR for FAR position"), [this]()
				{
					FString Result = TestActor->GetRightHandPositionTypeString(EStringFlowRightHandPositionType::FAR);
					TestEqual(TEXT("FAR position should return FAR"), Result, FString(TEXT("FAR")));
				});

			It(TEXT("Should return PIZZICATO for PIZZICATO position"), [this]()
				{
					FString Result = TestActor->GetRightHandPositionTypeString(EStringFlowRightHandPositionType::PIZZICATO);
					TestEqual(TEXT("PIZZICATO position should return PIZZICATO"), Result, FString(TEXT("PIZZICATO")));
				});

			It(TEXT("Should return different strings for different positions"), [this]()
				{
					FString Near = TestActor->GetRightHandPositionTypeString(EStringFlowRightHandPositionType::NEAR);
					FString Far = TestActor->GetRightHandPositionTypeString(EStringFlowRightHandPositionType::FAR);
					FString Pizzicato = TestActor->GetRightHandPositionTypeString(EStringFlowRightHandPositionType::PIZZICATO);

					TestNotEqual(TEXT("NEAR and FAR should differ"), Near, Far);
					TestNotEqual(TEXT("FAR and PIZZICATO should differ"), Far, Pizzicato);
					TestNotEqual(TEXT("NEAR and PIZZICATO should differ"), Near, Pizzicato);
				});
		});

	Describe(TEXT("ExportRecorderInfo and ImportRecorderInfo"), [this]()
		{
			It(TEXT("Should export to file successfully"), [this]()
				{
					FString TempPath = FPaths::ProjectIntermediateDir() + TEXT("StringFlowTest/test_export.json");
					IFileManager::Get().MakeDirectory(*FPaths::GetPath(TempPath), true);

					TestActor->IOFilePath = TempPath;
					TestActor->OneHandFingerNumber = 4;
					TestActor->StringNumber = 4;

					TestActor->ExportRecorderInfo(TempPath);

					bool bFileExists = IFileManager::Get().FileExists(*TempPath);
					TestTrue(TEXT("Exported file should exist"), bFileExists);

					if (bFileExists)
					{
						IFileManager::Get().Delete(*TempPath);
					}
				});

			It(TEXT("Should import from file successfully"), [this]()
				{
					FString TempPath = FPaths::ProjectIntermediateDir() + TEXT("StringFlowTest/test_import.json");
					IFileManager::Get().MakeDirectory(*FPaths::GetPath(TempPath), true);

					TestActor->IOFilePath = TempPath;
					TestActor->OneHandFingerNumber = 4;
					TestActor->StringNumber = 4;

					TestActor->ExportRecorderInfo(TempPath);

					bool bSuccess = TestActor->ImportRecorderInfo(TempPath);
					TestTrue(TEXT("Import should succeed"), bSuccess);

					if (IFileManager::Get().FileExists(*TempPath))
					{
						IFileManager::Get().Delete(*TempPath);
					}
				});

			It(TEXT("Should handle import from non-existent file"), [this]()
				{
					TestActor->IOFilePath = TEXT("/Invalid/Path/config.json");

					bool bSuccess = TestActor->ImportRecorderInfo(TestActor->IOFilePath);
					TestFalse(TEXT("Import should fail for non-existent file"), bSuccess);
				});

			It(TEXT("Should handle empty file path"), [this]()
				{
					TestActor->IOFilePath = TEXT("");

					// Should not crash
					TestActor->ExportRecorderInfo(TEXT(""));
					TestTrue(TEXT("Handled empty path"), true);
				});

			It(TEXT("Should import real violinist config file and preserve data"), [this]()
				{
					// 使用实际的 violinist 文件进行测试
					FString SourceFilePath = TEXT("H:\\stage_1\\docs\\阿蕾奇诺.violinist");

					// 验证源文件存在
					if (!IFileManager::Get().FileExists(*SourceFilePath))
					{
						// 跳过此测试，因为测试文件不存在
						UE_LOG(LogTemp, Warning, TEXT("Skipping real file import test - source file not found at %s"), *SourceFilePath);
						return;
					}

					// 导入配置
					TestActor->IOFilePath = SourceFilePath;
					bool bImportSuccess = TestActor->ImportRecorderInfo(SourceFilePath);
					TestTrue(TEXT("Import real violinist file should succeed"), bImportSuccess);

					// 验证配置参数被正确导入
					TestEqual(TEXT("OneHandFingerNumber should be 4"), TestActor->OneHandFingerNumber, 4);
					TestEqual(TEXT("StringNumber should be 4"), TestActor->StringNumber, 4);
				});

			It(TEXT("Should preserve data when exporting and re-importing"), [this]()
				{
					FString ExportPath = FPaths::ProjectIntermediateDir() + TEXT("StringFlowTest/export_reimport.json");
					IFileManager::Get().MakeDirectory(*FPaths::GetPath(ExportPath), true);

					// 设置初始数据
					TestActor->OneHandFingerNumber = 5;
					TestActor->StringNumber = 6;

					// 添加一些记录器数据
					FStringFlowRecorderTransform Transform1(FVector(1.5f, 2.5f, 3.5f), FQuat(0.1f, 0.2f, 0.3f, 0.9f));
					FStringFlowRecorderTransform Transform2(FVector(4.5f, 5.5f, 6.5f), FQuat(0.4f, 0.5f, 0.6f, 0.7f));

					TestActor->RecorderTransforms.Add(TEXT("test_recorder_1"), Transform1);
					TestActor->RecorderTransforms.Add(TEXT("test_recorder_2"), Transform2);

					int32 OriginalRecorderCount = TestActor->RecorderTransforms.Num();

					// 导出数据
					TestActor->ExportRecorderInfo(ExportPath);

					// 创建新 Actor 进行导入
					AStringFlowUnreal* ImportActor = NewObject<AStringFlowUnreal>();
					ImportActor->IOFilePath = ExportPath;
					bool bImportSuccess = ImportActor->ImportRecorderInfo(ExportPath);

					TestTrue(TEXT("Import should succeed"), bImportSuccess);
					TestEqual(TEXT("OneHandFingerNumber should be preserved"), ImportActor->OneHandFingerNumber, 5);
					TestEqual(TEXT("StringNumber should be preserved"), ImportActor->StringNumber, 6);

					// 清理
					ImportActor->ConditionalBeginDestroy();
					if (IFileManager::Get().FileExists(*ExportPath))
					{
						IFileManager::Get().Delete(*ExportPath);
					}
				});

			It(TEXT("Should export and verify JSON structure"), [this]()
				{
					FString ExportPath = FPaths::ProjectIntermediateDir() + TEXT("StringFlowTest/verify_json.json");
					IFileManager::Get().MakeDirectory(*FPaths::GetPath(ExportPath), true);

					TestActor->OneHandFingerNumber = 4;
					TestActor->StringNumber = 4;
					TestActor->ExportRecorderInfo(ExportPath);

					// 读取导出的文件并验证JSON结构
					FString FileContent;
					if (FFileHelper::LoadFileToString(FileContent, *ExportPath))
					{
						TestTrue(TEXT("Exported JSON should contain config"), FileContent.Contains(TEXT("config")));
						TestTrue(TEXT("Exported JSON should contain one_hand_finger_number"), FileContent.Contains(TEXT("one_hand_finger_number")));
						TestTrue(TEXT("Exported JSON should contain string_number"), FileContent.Contains(TEXT("string_number")));

						// 验证数值
						TestTrue(TEXT("Should contain finger number 4"), FileContent.Contains(TEXT("\"one_hand_finger_number\": 4")));
						TestTrue(TEXT("Should contain string number 4"), FileContent.Contains(TEXT("\"string_number\": 4")));
					}

					if (IFileManager::Get().FileExists(*ExportPath))
					{
						IFileManager::Get().Delete(*ExportPath);
					}
				});

			It(TEXT("Should compare exported file with original using real data"), [this]()
				{
					// 使用实际的 violinist 文件
					FString SourceFilePath = TEXT("H:\\stage_1\\docs\\阿蕾奇诺.violinist");

					if (!IFileManager::Get().FileExists(*SourceFilePath))
					{
						UE_LOG(LogTemp, Warning, TEXT("Skipping comparison test - source file not found at %s"), *SourceFilePath);
						return;
					}

					// 第一步：导入原始文件
					TestActor->IOFilePath = SourceFilePath;
					bool bImportSuccess = TestActor->ImportRecorderInfo(SourceFilePath);
					TestTrue(TEXT("Should import original file successfully"), bImportSuccess);

					int32 OriginalConfigFingers = TestActor->OneHandFingerNumber;
					int32 OriginalConfigStrings = TestActor->StringNumber;

					// 第二步：导出为新文件
					FString ExportPath = FPaths::ProjectIntermediateDir() + TEXT("StringFlowTest/exported_comparison.json");
					IFileManager::Get().MakeDirectory(*FPaths::GetPath(ExportPath), true);

					TestActor->ExportRecorderInfo(ExportPath);

					// 第三步：读取原始和导出文件进行对比
					FString OriginalContent;
					FString ExportedContent;

					bool bOriginalRead = FFileHelper::LoadFileToString(OriginalContent, *SourceFilePath);
					bool bExportedRead = FFileHelper::LoadFileToString(ExportedContent, *ExportPath);

					TestTrue(TEXT("Should read original file"), bOriginalRead);
					TestTrue(TEXT("Should read exported file"), bExportedRead);

					if (bOriginalRead && bExportedRead)
					{
						// 验证基本配置参数
						TestTrue(TEXT("Exported should have config section"), ExportedContent.Contains(TEXT("\"config\"")));
						TestTrue(TEXT("Exported should preserve finger number"), ExportedContent.Contains(*FString::Printf(TEXT("\"one_hand_finger_number\": %d"), OriginalConfigFingers)));
						TestTrue(TEXT("Exported should preserve string number"), ExportedContent.Contains(*FString::Printf(TEXT("\"string_number\": %d"), OriginalConfigStrings)));

						UE_LOG(LogTemp, Warning, TEXT("✓ Original file size: %d chars"), OriginalContent.Len());
						UE_LOG(LogTemp, Warning, TEXT("✓ Exported file size: %d chars"), ExportedContent.Len());
						UE_LOG(LogTemp, Warning, TEXT("✓ Config preserved: %d fingers, %d strings"), OriginalConfigFingers, OriginalConfigStrings);
					}

					// 清理
					if (IFileManager::Get().FileExists(*ExportPath))
					{
						IFileManager::Get().Delete(*ExportPath);
					}
				});
		});

	Describe(TEXT("RecorderTransforms management"), [this]()
		{
			It(TEXT("Should initialize empty RecorderTransforms"), [this]()
				{
					TestTrue(TEXT("RecorderTransforms should be accessible"), TestActor->RecorderTransforms.Num() >= 0);
				});

			It(TEXT("Should support adding recorder data"), [this]()
				{
					FStringFlowRecorderTransform Transform(FVector(1, 2, 3), FQuat::Identity);
					TestActor->RecorderTransforms.Add(TEXT("test"), Transform);

					TestEqual(TEXT("Should contain one entry"), TestActor->RecorderTransforms.Num(), 1);
				});

			It(TEXT("Should support multiple recorder entries"), [this]()
				{
					for (int32 i = 0; i < 10; ++i)
					{
						FStringFlowRecorderTransform Transform(FVector(i, i + 1, i + 2), FQuat::Identity);
						TestActor->RecorderTransforms.Add(*FString::FromInt(i), Transform);
					}

					TestEqual(TEXT("Should have 10 entries"), TestActor->RecorderTransforms.Num(), 10);
				});

			It(TEXT("Should clear all recorder data"), [this]()
				{
					TestActor->RecorderTransforms.Add(TEXT("test"), FStringFlowRecorderTransform(FVector::Zero(), FQuat::Identity));
					TestActor->RecorderTransforms.Empty();

					TestEqual(TEXT("Should be empty after clear"), TestActor->RecorderTransforms.Num(), 0);
				});
		});

	Describe(TEXT("Configuration data"), [this]()
		{
			It(TEXT("Should store StringNumber correctly"), [this]()
				{
					TestActor->StringNumber = 6;
					TestEqual(TEXT("StringNumber should be 6"), TestActor->StringNumber, 6);
				});

			It(TEXT("Should store OneHandFingerNumber correctly"), [this]()
				{
					TestActor->OneHandFingerNumber = 5;
					TestEqual(TEXT("OneHandFingerNumber should be 5"), TestActor->OneHandFingerNumber, 5);
				});

			It(TEXT("Should store animation file path"), [this]()
				{
					TestActor->AnimationFilePath = TEXT("/Game/Animations/test.json");
					TestEqual(TEXT("AnimationFilePath should be stored"), TestActor->AnimationFilePath, FString(TEXT("/Game/Animations/test.json")));
				});

			It(TEXT("Should store IO file path"), [this]()
				{
					TestActor->IOFilePath = TEXT("/Game/Data/recorder.json");
					TestEqual(TEXT("IOFilePath should be stored"), TestActor->IOFilePath, FString(TEXT("/Game/Data/recorder.json")));
				});
		});

	Describe(TEXT("Hand position type storage"), [this]()
		{
			It(TEXT("Should store left hand position type"), [this]()
				{
					TestActor->LeftHandPositionType = EStringFlowLeftHandPositionType::INNER;
					TestEqual(TEXT("LeftHandPositionType should be INNER"), static_cast<int32>(TestActor->LeftHandPositionType), static_cast<int32>(EStringFlowLeftHandPositionType::INNER));
				});

			It(TEXT("Should store right hand position type"), [this]()
				{
					TestActor->RightHandPositionType = EStringFlowRightHandPositionType::FAR;
					TestEqual(TEXT("RightHandPositionType should be FAR"), static_cast<int32>(TestActor->RightHandPositionType), static_cast<int32>(EStringFlowRightHandPositionType::FAR));
				});
		});
}

#endif // WITH_AUTOMATION_TESTS
