// Copyright Cody McCarty.

#pragma once

#include "CoreMinimal.h"
#include "ModularPlayerState.h"
#include "StratPlayerState.generated.h"

/**
 * todo doc
 */
UCLASS(meta=(PrioritizeCategories="User"))
class UE_RTS_API AStratPlayerState : public AModularPlayerState
{
	GENERATED_BODY()

public:
	AStratPlayerState();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginReplication() override;
	// virtual void ClientInitialize(AController* C) override;
	// virtual void BeginPlay() override;

	/** Player Color is a quick way other players identify another player. Can be used in text and decals. */
	UFUNCTION(BlueprintPure, Category=StratPlayerState)
	FLinearColor GetPlayerColor() const { return PlayerColor; }

	/** Player Color is a quick way other players identify another player. Can be used in text and decals. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category=StratPlayerState, meta=(AutoCreateRefTerm="NewPlayerColor"))
	void SetPlayerColor(const FLinearColor& NewPlayerColor);

protected:
	/** Player Color is a quick way other players identify another player. Can be used in text and decals. */
	UPROPERTY(EditInstanceOnly, ReplicatedUsing=OnRep_PlayerColor, Category="User|Options", Getter, Setter)
	FLinearColor PlayerColor{FLinearColor::Gray};
	UFUNCTION()
	void OnRep_PlayerColor(const FLinearColor& OldPlayerColor);
	void BroadcastPlayerColorChanged(const FLinearColor& OldPlayerColor);
	UFUNCTION(Server, Reliable)
	void Server_SetPlayerColor(const FLinearColor& NewPlayerColor);
};
