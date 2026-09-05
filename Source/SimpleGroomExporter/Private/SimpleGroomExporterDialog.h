#pragma once
#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Widgets/SWindow.h"
#include "Modules/ModuleManager.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Input/SButton.h"
#include "Framework/Application/SlateApplication.h"
#include "Styling/AppStyle.h"
#include "SimpleGroomExporterDialog.generated.h"

#define LOCTEXT_NAMESPACE "SimpleGroomExporter"

// One Alembic output axis = one UE position component (optionally negated).
UENUM()
enum class EGroomExportAxis : uint8
{
	X     UMETA(DisplayName = "X"),
	NegX  UMETA(DisplayName = "-X"),
	Y     UMETA(DisplayName = "Y"),
	NegY  UMETA(DisplayName = "-Y"),
	Z     UMETA(DisplayName = "Z"),
	NegZ  UMETA(DisplayName = "-Z"),
};

// Coordinate mapping preset (UE -> Alembic).
UENUM()
enum class EGroomExportAxisPreset : uint8
{
	// Raw UE coordinates, no swap (matches the original kostenickj exporter).
	Default   UMETA(DisplayName = "Default (UE raw: X, Y, Z)"),
	// Mapping for the Unity pipeline: Alembic(X, Y, Z) = UE(X, Z, Y).
	ForUnity  UMETA(DisplayName = "For Unity (X, Z, Y)"),
	// Manual mapping: each output axis is chosen individually below.
	Custom    UMETA(DisplayName = "Custom"),
};

UCLASS(BlueprintType, config = EditorPerProjectUserSettings, Category = "Groom Export")
class SIMPLEGROOMEXPORTER_API USimpleGroomExportOptions : public UObject
{
	public:
		GENERATED_BODY()

		UPROPERTY(EditAnywhere, config, Category = "Export Options", meta = (DisplayName = "Use Render Data", ToolTip = "Export the render/simulation strands the groom actually renders (guide->strand interpolation and grooming applied) instead of the raw imported strands. The groom must NOT be decimated. Can fix a few export issues but produces more strands."))
		bool bUseRenderData = false;

		// --- Coordinate Mapping ---
		UPROPERTY(EditAnywhere, config, Category = "Coordinate Mapping", meta = (DisplayName = "Axis Preset", ToolTip = "How UE axes are converted to Alembic. Custom lets you assign each output axis manually."))
		EGroomExportAxisPreset AxisPreset = EGroomExportAxisPreset::Default;

		UPROPERTY(EditAnywhere, config, Category = "Coordinate Mapping", meta = (DisplayName = "Out X =", EditCondition = "AxisPreset == EGroomExportAxisPreset::Custom", EditConditionHides))
		EGroomExportAxis CustomOutX = EGroomExportAxis::X;

		UPROPERTY(EditAnywhere, config, Category = "Coordinate Mapping", meta = (DisplayName = "Out Y =", EditCondition = "AxisPreset == EGroomExportAxisPreset::Custom", EditConditionHides))
		EGroomExportAxis CustomOutY = EGroomExportAxis::Z;

		UPROPERTY(EditAnywhere, config, Category = "Coordinate Mapping", meta = (DisplayName = "Out Z =", EditCondition = "AxisPreset == EGroomExportAxisPreset::Custom", EditConditionHides))
		EGroomExportAxis CustomOutZ = EGroomExportAxis::Y;

		// --- Attributes (extra data that Unity reads) ---
		UPROPERTY(EditAnywhere, config, Category = "Attributes", meta = (DisplayName = "Export Widths", ToolTip = "Write the strand diameter, in centimeters, as the groom renders it."))
		bool bExportWidths = true;

		UPROPERTY(EditAnywhere, config, Category = "Attributes", meta = (DisplayName = "Export Root UV", ToolTip = "Write each strand's root UV on every one of its vertices."))
		bool bExportRootUV = true;
};

class SSimpleGroomExporterExportDialog : public SWindow
{
	public:
		struct FResult
		{
			EAppReturnType::Type RetType;
			USimpleGroomExportOptions* Options = nullptr;

			FResult() :
			RetType(EAppReturnType::Type::Cancel)
			{
			}
		};

		SLATE_BEGIN_ARGS(SSimpleGroomExporterExportDialog)
			{
			}

		SLATE_END_ARGS()

		SSimpleGroomExporterExportDialog();

		void Construct(const FArguments& InArgs);

		FResult ShowModal();

	private:
		USimpleGroomExportOptions* Config;
		TSharedPtr<IDetailsView> SettingsPanel;
		FResult Result;

		FReply OnButtonClick(EAppReturnType::Type ButtonID);
};

#undef LOCTEXT_NAMESPACE
