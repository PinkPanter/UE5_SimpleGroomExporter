// SimpleGroomExporter.cpp

#include "SimpleGroomExporter.h"
#include "SimpleGroomExporterDialog.h"

#include "HAL/Platform.h"
#include "GroomAsset.h"
#include "Logging/LogMacros.h"
#include "Misc/Paths.h"
#include "Misc/FeedbackContext.h"
#include "Serialization/Archive.h"
#include "UObject/Object.h"

#if PLATFORM_WINDOWS
#include "Windows/AllowWindowsPlatformTypes.h"
#endif

THIRD_PARTY_INCLUDES_START
#include <Alembic/AbcGeom/All.h>
#include <Alembic/AbcCoreAbstract/All.h>
#include <Alembic/AbcCoreFactory/All.h>
#include <Alembic/AbcCoreOgawa/All.h>
THIRD_PARTY_INCLUDES_END

#if PLATFORM_WINDOWS
#include "Windows/HideWindowsPlatformTypes.h"
#endif

using namespace Alembic::Abc;
using namespace Alembic::AbcGeom;

// Define a logging category for the exporter
DEFINE_LOG_CATEGORY_STATIC(LogSimpleGroomExporter, Log, All);

#define LOCTEXT_NAMESPACE "SimpleGroomExporter"

// Extract one UE position component for the chosen output axis (optionally negated).
static float ResolveAxisComponent(const FVector3f& P, EGroomExportAxis Axis)
{
	switch (Axis)
	{
		case EGroomExportAxis::X:    return  P.X;
		case EGroomExportAxis::NegX: return -P.X;
		case EGroomExportAxis::Y:    return  P.Y;
		case EGroomExportAxis::NegY: return -P.Y;
		case EGroomExportAxis::Z:    return  P.Z;
		case EGroomExportAxis::NegZ: return -P.Z;
		default:                     return  0.0f;
	}
}

// Expand a preset into three concrete axis mappings (Custom reads them straight from the options).
static void ResolveAxisMapping(const USimpleGroomExportOptions* Options,
	EGroomExportAxis& OutX, EGroomExportAxis& OutY, EGroomExportAxis& OutZ)
{
	switch (Options->AxisPreset)
	{
		case EGroomExportAxisPreset::Custom:
			OutX = Options->CustomOutX; OutY = Options->CustomOutY; OutZ = Options->CustomOutZ; break;
		case EGroomExportAxisPreset::ForUnity: // mapping for the Unity pipeline: Alembic(X, Y, Z) = UE(X, Z, Y)
			OutX = EGroomExportAxis::X; OutY = EGroomExportAxis::Z; OutZ = EGroomExportAxis::Y; break;
		case EGroomExportAxisPreset::Default: // raw UE coordinates
		default:
			OutX = EGroomExportAxis::X; OutY = EGroomExportAxis::Y; OutZ = EGroomExportAxis::Z; break;
	}
}

// Human-readable axis label for logging.
static const TCHAR* AxisName(EGroomExportAxis Axis)
{
	switch (Axis)
	{
		case EGroomExportAxis::X:    return TEXT("X");
		case EGroomExportAxis::NegX: return TEXT("-X");
		case EGroomExportAxis::Y:    return TEXT("Y");
		case EGroomExportAxis::NegY: return TEXT("-Y");
		case EGroomExportAxis::Z:    return TEXT("Z");
		case EGroomExportAxis::NegZ: return TEXT("-Z");
		default:                     return TEXT("?");
	}
}

USimpleGroomExporter::USimpleGroomExporter()
{
	SupportedClass = UGroomAsset::StaticClass();
	bText = false;
	FormatExtension.Add(TEXT("ABC"));
	FormatDescription.Add(TEXT("Alembic File"));
}

bool USimpleGroomExporter::ExportBinary(UObject* Object, const TCHAR* Type, FArchive& Ar, FFeedbackContext* Warn, int32 FileIndex, uint32 PortFlags)
{
	// Validate input object
	if (!Object)
	{
		UE_LOG(LogSimpleGroomExporter, Error, TEXT("Export failed: Object is null."));
		return false;
	}

	// Ensure the object is of type UGroomAsset
	UGroomAsset* Groom = Cast<UGroomAsset>(Object);
	if (!Groom)
	{
		UE_LOG(LogSimpleGroomExporter, Error, TEXT("Export failed: Object is not a UGroomAsset."));
		return false;
	}

	// Retrieve the output filename
	FString OutputFileName = GetExportFilePath();
	if (OutputFileName.IsEmpty())
	{
		UE_LOG(LogSimpleGroomExporter, Error, TEXT("Export failed: Output file path is empty."));
		return false;
	}

	// Show export options modal dialog
	TSharedRef<SSimpleGroomExporterExportDialog> OptionsModal = SNew(SSimpleGroomExporterExportDialog);
	SSimpleGroomExporterExportDialog::FResult ExportOptions = OptionsModal->ShowModal();
	if (ExportOptions.RetType == EAppReturnType::Cancel)
	{
		return false;
	}

	// Initialize Alembic archive
	try
	{
		// Convert FString to std::string using UTF-8 encoding
		std::string OutputFileNameStd = TCHAR_TO_UTF8(*OutputFileName);
		OArchive Archive(Alembic::AbcCoreOgawa::WriteArchive(), OutputFileNameStd);
		OObject TopLevelObj = Archive.getTop();

		// Retrieve hair groups from the groom asset
		const FHairDescriptionGroups& HairGroups = Groom->GetHairDescriptionGroups();


		if (HairGroups.HairGroups.Num() == 0)
		{
			UE_LOG(LogSimpleGroomExporter, Warning, TEXT("No hair groups found in the groom asset."));
			return false;
		}

		// Use the options selected by the user from the modal dialog
		const USimpleGroomExportOptions* Options = ExportOptions.Options;
		const bool bUseRenderdataForExport = Options->bUseRenderData;

		// Resolve the coordinate mapping (preset -> three axes) once for the whole export.
		EGroomExportAxis MapOutX, MapOutY, MapOutZ;
		ResolveAxisMapping(Options, MapOutX, MapOutY, MapOutZ);
		UE_LOG(LogSimpleGroomExporter, Log, TEXT("Axis mapping: Alembic(X,Y,Z) = UE(%s, %s, %s). ExportWidths=%s, ExportRootUV=%s"),
			AxisName(MapOutX), AxisName(MapOutY), AxisName(MapOutZ),
			Options->bExportWidths ? TEXT("YES") : TEXT("NO"),
			Options->bExportRootUV ? TEXT("YES") : TEXT("NO"));

		// TODO, consider using raw data hair desc data to export
		// see how GenerateHairCardGenGroomData access it....
		auto HairDesc = Groom->GetHairDescription();

		// Iterate over each hair group
		int32 GroupIdx = 0;
		for (const FHairDescriptionGroup& HairGroupInfo : HairGroups.HairGroups)
		{
			const FHairStrandsRawDatas& RawStrandData = HairGroupInfo.Strands;
			FHairStrandsDatas StrandData;
			FHairStrandsDatas GuidesData;
			if (bUseRenderdataForExport)
			{
				Groom->GetHairStrandsDatas(GroupIdx, StrandData, GuidesData);
			}

			const FHairStrandsCurves& StrandCurves = bUseRenderdataForExport ? StrandData.StrandsCurves : RawStrandData.StrandsCurves;
			const FHairStrandsPoints& StrandPoints = bUseRenderdataForExport ? StrandData.StrandsPoints : RawStrandData.StrandsPoints;
			const TArray<FVector3f>& Positions = StrandPoints.PointsPosition;

			// Prepare containers for Alembic data
			std::vector<Imath::V3f> AllPositions;
			std::vector<int32_t> VerticesPerStrand;

			// Standard Alembic attributes
			// AllWidths - per-vertex strand diameter (geoScope = vertex), parallel to AllPositions.
			// RootUVs   - per-strand root UV (geoScope = uniform), one entry per EMITTED strand.
			std::vector<float> AllWidths;
			std::vector<Imath::V2f> RootUVs;

			AllPositions.reserve(Positions.Num());
			VerticesPerStrand.reserve(StrandCurves.CurvesCount.Num());
			AllWidths.reserve(Positions.Num());
			RootUVs.reserve(StrandCurves.CurvesCount.Num());


			const TArray<uint16>& CurvesCounts = StrandCurves.CurvesCount;
			const TArray<uint32>& CurvesOffsets = StrandCurves.CurvesOffset;
			const int32 NumStrands = CurvesCounts.Num();

			// Sources for the new attributes (may be empty -> attribute is skipped instead of crashing).
			const TArray<float>& PointsRadius = StrandPoints.PointsRadius;   // [0..1], parallel to PointsPosition
			const TArray<FVector2f>& CurvesRootUV = StrandCurves.CurvesRootUV; // one entry per strand

			// Write width only if the option is enabled AND there is a radius for every vertex we read (parallel to positions).
			bool bWriteWidths = false;
			if (Options->bExportWidths)
			{
				bWriteWidths = (PointsRadius.Num() == Positions.Num()) && (PointsRadius.Num() > 0);
				if (!bWriteWidths)
				{
					UE_LOG(LogSimpleGroomExporter, Warning, TEXT("Group %d: PointsRadius size (%d) != PointsPosition size (%d). Exporting WITHOUT 'width' attribute."), GroupIdx, PointsRadius.Num(), Positions.Num());
				}
			}
			// Write uv only if the option is enabled AND there is exactly one root UV per strand.
			bool bWriteUVs = false;
			if (Options->bExportRootUV)
			{
				bWriteUVs = (CurvesRootUV.Num() == NumStrands) && (NumStrands > 0);
				if (!bWriteUVs)
				{
					UE_LOG(LogSimpleGroomExporter, Warning, TEXT("Group %d: CurvesRootUV size (%d) != strand count (%d). Exporting WITHOUT 'uv' attribute."), GroupIdx, CurvesRootUV.Num(), NumStrands);
				}
			}

			int32 GroupVertexIndex = 0;
			for (int32 StrandIndex = 0; StrandIndex < NumStrands; ++StrandIndex)
			{
				uint32 NumVertsInStrand = CurvesCounts[StrandIndex];
				if (bUseRenderdataForExport)
				{
					NumVertsInStrand = NumVertsInStrand - 1;
				}
				if (NumVertsInStrand <= 0)
				{
					UE_LOG(LogSimpleGroomExporter, Warning, TEXT("Strand %d has non-positive vertex count (%d). Skipping."), StrandIndex, NumVertsInStrand);
					continue;
				}

				// Collect positions for the current strand, curve offsets only seem to be set when using RENDER data, not hair strands raw datas
				uint32 StartVertexIndex = CurvesOffsets[StrandIndex];

				for (uint32 VertexIndexInStrand = 0; VertexIndexInStrand < NumVertsInStrand; ++VertexIndexInStrand)
				{
					uint32 CurrentVertexIndex = bUseRenderdataForExport ? StartVertexIndex + VertexIndexInStrand : GroupVertexIndex;
					if (CurrentVertexIndex >= static_cast<uint32>(Positions.Num()))
					{
						UE_LOG(LogSimpleGroomExporter, Error, TEXT("Vertex index out of bounds: %d >= %d"), CurrentVertexIndex, Positions.Num());
						return false;
					}
					const FVector3f& Pos = Positions[CurrentVertexIndex];
					// UE -> Alembic axis mapping per the selected preset/custom (see ResolveAxisMapping).
					AllPositions.emplace_back(
						ResolveAxisComponent(Pos, MapOutX),
						ResolveAxisComponent(Pos, MapOutY),
						ResolveAxisComponent(Pos, MapOutZ));

					// width is a scalar (radius), no coordinate transform. Same CurrentVertexIndex as positions keeps the arrays parallel.
					if (bWriteWidths)
					{
						AllWidths.emplace_back(PointsRadius[CurrentVertexIndex]);
					}

					GroupVertexIndex++;
				}

				VerticesPerStrand.emplace_back(NumVertsInStrand);

				// One root UV per EMITTED strand
				if (bWriteUVs)
				{
					const FVector2f& RootUV = CurvesRootUV[StrandIndex];
					RootUVs.emplace_back(RootUV.X, RootUV.Y);
				}
			}

			// width -> geoScope = kVertexScope  (one diameter per vertex, parallel to positions).
			OFloatGeomParam::Sample widthSample;
			if (bWriteWidths)
			{
				widthSample = OFloatGeomParam::Sample(Alembic::Abc::FloatArraySample(AllWidths.data(), AllWidths.size()), kVertexScope);
			}

			// uv    -> geoScope = kUniformScope (2 floats per strand, one root UV per strand).
			OV2fGeomParam::Sample uvSample;
			if (bWriteUVs)
			{
				uvSample = OV2fGeomParam::Sample(V2fArraySample(RootUVs.data(), RootUVs.size()), kUniformScope);
			}

			// Create Alembic curve sample
			OCurvesSchema::Sample CurveSample
			(
				V3fArraySample(AllPositions.data(), AllPositions.size())
				, Int32ArraySample(VerticesPerStrand.data(), VerticesPerStrand.size())
				, kCubic // Type of curve; can be adjusted as needed
				, kNonPeriodic
				, widthSample
				, uvSample
			);

			// Define the name for the Alembic curves object
			std::string CurveName = std::string(TCHAR_TO_UTF8(*HairGroupInfo.Info.GroupName.ToString()));

			// Create the Alembic curves object and set its schema
			OCurves CurvesObj(TopLevelObj, CurveName);
			OCurvesSchema& CurvesSchema = CurvesObj.getSchema();
			CurvesSchema.set(CurveSample);

			// Per-group export report: geometry counts and which attributes were written.
			UE_LOG(LogSimpleGroomExporter, Log, TEXT("Exported hair group %d '%s': %llu strands, %llu verts, width=%s, uv=%s"),
				GroupIdx,
				*HairGroupInfo.Info.GroupName.ToString(),
				static_cast<uint64>(VerticesPerStrand.size()),
				static_cast<uint64>(AllPositions.size()),
				bWriteWidths ? TEXT("YES") : TEXT("NO"),
				bWriteUVs ? TEXT("YES") : TEXT("NO"));
			GroupIdx++;
		}

		UE_LOG(LogSimpleGroomExporter, Log, TEXT("Alembic export completed successfully: %s"), *OutputFileName);
		return true;
	}
	catch (const std::exception& Ex)
	{
		UE_LOG(LogSimpleGroomExporter, Error, TEXT("Alembic export failed: %s"), UTF8_TO_TCHAR(Ex.what()));
		return false;
	}
	catch (...)
	{
		UE_LOG(LogSimpleGroomExporter, Error, TEXT("Alembic export failed due to an unknown error."));
		return false;
	}
}

FString USimpleGroomExporter::GetExportFilePath() const
{
	return UExporter::CurrentFilename;
}

#undef LOCTEXT_NAMESPACE
