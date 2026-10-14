// Copyright Epic Games, Inc. All Rights Reserved.

#include "World/L2H_StationMarker.h"

#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"

UL2H_StationMarker::UL2H_StationMarker()
{
	PrimaryComponentTick.bCanEverTick = false;
}

UStaticMeshComponent* UL2H_StationMarker::MakePiece(const TCHAR* Name)
{
	UStaticMeshComponent* Piece = NewObject<UStaticMeshComponent>(GetOwner(), Name, RF_Transient);
	Piece->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder")));
	Piece->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Piece->SetCastShadow(false);
	Piece->SetupAttachment(this);
	Piece->RegisterComponent();
	return Piece;
}

void UL2H_StationMarker::OnRegister()
{
	Super::OnRegister();

	if (!GetOwner() || Pad)
	{
		Refresh();
		return;
	}

	// The engine cylinder is 100 cm across and 100 cm tall, centred on its origin.
	Pad = MakePiece(TEXT("MarkerPad"));
	Pole = MakePiece(TEXT("MarkerPole"));

	// One label: the text material draws both sides.
	Text = NewObject<UTextRenderComponent>(GetOwner(), TEXT("MarkerLabel"), RF_Transient);
	Text->SetupAttachment(this);
	Text->SetHorizontalAlignment(EHTA_Center);
	Text->SetVerticalAlignment(EVRTA_TextBottom);
	Text->SetWorldSize(42.0f);
	Text->RegisterComponent();

	Refresh();
}

void UL2H_StationMarker::Refresh()
{
	if (!Pad || !Pole || !Text)
	{
		return;
	}

	Pad->SetRelativeLocation(FVector(0.0f, 0.0f, 1.0f));
	Pad->SetRelativeScale3D(FVector(PadRadius / 50.0f, PadRadius / 50.0f, 0.02f));
	Pole->SetRelativeLocation(FVector(PadRadius * 0.8f, 0.0f, PoleHeight * 0.5f));
	Pole->SetRelativeScale3D(FVector(0.25f, 0.25f, PoleHeight / 100.0f));
	Text->SetRelativeLocation(FVector(PadRadius * 0.8f, 0.0f, PoleHeight + 20.0f));
	Text->SetText(Label);
	Text->SetTextRenderColor(Color.ToFColor(true));

	// The engine's basic-shape material takes a "Color" parameter; one instance per piece.
	if (UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")))
	{
		for (UStaticMeshComponent* Piece : { Pad.Get(), Pole.Get() })
		{
			UMaterialInstanceDynamic* Mid = UMaterialInstanceDynamic::Create(Base, Piece);
			Mid->SetVectorParameterValue(TEXT("Color"), Color);
			Piece->SetMaterial(0, Mid);
		}
	}
}
