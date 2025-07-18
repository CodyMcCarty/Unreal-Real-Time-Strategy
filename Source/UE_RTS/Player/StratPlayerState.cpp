// Copyright Cody McCarty.


#include "StratPlayerState.h"

#include "SandCoreLogToolsBPLibrary.h"
#include "Net/UnrealNetwork.h"

namespace
{
	FLinearColor Debug_MakeColorFromPieId(const int32 PieId)
	{
		FLinearColor NewPlayerColor(FMath::FRand(), FMath::FRand(), FMath::FRand(), 1.f);
#if WITH_EDITOR
		switch (PieId)
		{
		case 0: NewPlayerColor = FLinearColor::Red;
			break;
		case 1: NewPlayerColor = FLinearColor::Green;
			break;
		case 2: NewPlayerColor = FLinearColor::Blue;
			break;
		default: NewPlayerColor = FLinearColor::Yellow;
		}
#endif

		return NewPlayerColor;
	}
}

AStratPlayerState::AStratPlayerState()
{
}

void AStratPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AStratPlayerState, PlayerColor);
}

void AStratPlayerState::BeginReplication()
{
	Super::BeginReplication();

	const APlayerController* PC = GetPlayerController();
	if (PC && PC->IsLocalController())
	{
		const FLinearColor MakeColorFromPieId = Debug_MakeColorFromPieId(UE::GetPlayInEditorID());
		if (HasAuthority())
		{
			SetPlayerColor(MakeColorFromPieId);
		}
		else
		{
			Server_SetPlayerColor(MakeColorFromPieId);
		}
	}
}

void AStratPlayerState::SetPlayerColor(const FLinearColor& NewPlayerColor)
{
	if (NewPlayerColor != PlayerColor)
	{
		const FLinearColor OldPlayerColor = PlayerColor;
		PlayerColor = NewPlayerColor;
		BroadcastPlayerColorChanged(OldPlayerColor);
	}
}

void AStratPlayerState::OnRep_PlayerColor(const FLinearColor& OldPlayerColor)
{
	if (PlayerColor != OldPlayerColor)
	{
		BroadcastPlayerColorChanged(OldPlayerColor);
	}
}

void AStratPlayerState::BroadcastPlayerColorChanged(const FLinearColor& OldPlayerColor)
{
	INFO_LOG(LogTemp, Warning, TEXT("NewColor=%s  OldColor=%s"), *PlayerColor.ToString(), *OldPlayerColor.ToString())
}

void AStratPlayerState::Server_SetPlayerColor_Implementation(const FLinearColor& NewPlayerColor) { SetPlayerColor(NewPlayerColor); }
