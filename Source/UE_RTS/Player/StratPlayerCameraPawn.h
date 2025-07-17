// Copyright Cody McCarty.

#pragma once

#include "CoreMinimal.h"
#include "InputAction.h"
#include "ModularPawn.h"
#include "StratPlayerCameraPawn.generated.h"

class UCameraComponent;
class USpringArmComponent;
class UInputAction;
class UInputMappingContext;

DECLARE_LOG_CATEGORY_EXTERN(LogGame, Log, All);

/** Replicated movement data. Simplified version of FRepMovement more suited to an RTS camera.  */
USTRUCT()
struct FSimpleRepMovement
{
	GENERATED_BODY()

	UPROPERTY(Transient, VisibleInstanceOnly)
	FVector Location{FVector::ZeroVector};

	UPROPERTY(Transient, VisibleInstanceOnly)
	float Yaw{0};

	/** Is used to compare out of data packets. We only want the most recent transform. Increment this before sending. */
	UPROPERTY(Transient, VisibleInstanceOnly)
	uint32 ServerFrame{0};
};

/** Responsible for moving the player around the map. Feels more like a Tycoon game than an RTS. Smooth movement even in bad network emulation. */
UCLASS()
class UE_RTS_API AStratPlayerCameraPawn : public AModularPawn
{
	GENERATED_BODY()
#pragma region Lifecycle

public:
	AStratPlayerCameraPawn();
	virtual void GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const override;
	// virtual void PostInitializeComponents() override;

protected:
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;
	virtual void BeginPlay() override;

public:
	virtual void NotifyControllerChanged() override;
	// virtual void PossessedBy(AController* NewController) override;
	virtual void OnRep_Controller() override;
	// virtual void OnPlayerStateChanged(APlayerState* NewPlayerState, APlayerState* OldPlayerState) override;
	// virtual void OnRep_PlayerState() override;
	virtual void Tick(float DeltaTime) override;
#pragma endregion

protected:
	void TimerLoop_TraceForHeight();
	void TimerLoop_ServerSetSimpleRepMovement();
	bool TraceForCamCollision(FHitResult& OutHit, const FVector& CamLoc) const; 
	void Move(const FInputActionInstance& InputActionInstance);
	void Zoom(const FInputActionInstance& InputActionInstance);
	void RotateStarted(const FInputActionInstance& InputActionInstance);
	void Rotate(const FInputActionInstance& InputActionInstance);
	void EnableRotateStarted(const FInputActionInstance& InputActionInstance);
	void EnableRotateEnded(const FInputActionInstance& InputActionInstance);
	
	UPROPERTY()
	TObjectPtr<USpringArmComponent> SpringArmComp;

	/** Is the target transform of locally controlled pawns OR the last received transform for other net roles. Visible in PIE for debugging. */
	UPROPERTY(VisibleInstanceOnly, Category="User|Info", ReplicatedUsing=OnRep_SimpleRepMovement)
	FSimpleRepMovement SimpleRepMovement;
	
	UFUNCTION()
	void OnRep_SimpleRepMovement(const FSimpleRepMovement& OldSimpleRepMovement);

	UFUNCTION(Server, Unreliable)
	void Server_SetSimpleRepMovementState(const FSimpleRepMovement NewSimpleRepMovement);
	
	FHitResult GroundHit;
	FTimerHandle SendSimpleRepMovement_TimerHandle;
	FTimerHandle TraceForHeight_TimerHandle;
	FVector TargetMoveLoc;
	FIntPoint MousePosSnapshot;
	
	/** This value is calculated. Based on Zoom, Min&MaxZoom, and Min&MaxMoveSpeed. Zoomed out goes faster. */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="User|Info", meta=(Units="cm/s"))
	float MoveSpeedCalculated{800.f};

	/** Zoom. The length of the SpringArm after interpolation. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="User|Info", meta=(Units="cm"))
	float ZoomArmLength{800.f};

#pragma region BPConfig
	// todo: consider having a BPConfig data asset.
protected:
	/** If supplied, will be added with 1 priority. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="User|Options")
	TObjectPtr<UInputMappingContext> CameraMovementInputMapping;

	/** Expects an Axis2D (Vector2D). Normally WASD or ESDF */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="User|Options")
	TObjectPtr<UInputAction> Input_Move;

	/** Expects Axis1D (float). Normally mouse wheel axis in/out */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="User|Options")
	TObjectPtr<UInputAction> Input_Zoom;

	/** Expects an Axis2D (Vector2D). Normally mouse XY  */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="User|Options")
	TObjectPtr<UInputAction> Input_Rotate;

	/** Expects a bool. Normally mmb and/or ctrl, alt */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="User|Options")
	TObjectPtr<UInputAction> Input_EnableRotate;

	/** If valid, limits camera movement bounds, so the player doesn't leave the play space. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="User|Options")
	FBox2D MapBounds{FVector2D(-80000.f, -80000.f), FVector2D(80000.f, 80000.f)};

	/** Controls how quickly the camera reaches target position. Low values are slower (more lag), high values are faster (less lag), while zero is instant (no lag). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="User|Options", meta=(ClampMin="0.0", ClampMax="1000.0", UIMin="0.0", UIMax="50.0"))
	float CameraLagSpeed{5.f};

	/** Zoom adjusts speed. This is the speed when zoomed all the way in. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="User|Options", meta=(ClampMin="10.0", UIMin="100.0", UIMax="500.0", Units="cm/s"))
	float MinMoveSpeed{200.f};

	/** Zoom adjusts speed. This is the speed when zoomed all the way out. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="User|Options", meta=(ClampMin="10.0", UIMin="1000.0", UIMax="20000.0", Units="cm/s"))
	float MaxMoveSpeed{4000.f};

	/** This also affects speed */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="User|Options", meta=(ClampMin="5.0", UIMin="50.0", UIMax="500.0", Units="cm"))
	float MinZoom{200.f};

	/** This also affects speed */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="User|Options", meta=(ClampMin="5.0", UIMin="1000.0", UIMax="8000.0", Units="cm"))
	float MaxZoom{3000.f};

	/** Controls how quickly the camera reaches target position. Low values are slower (more lag), high values are faster (less lag), while zero is instant (no lag). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="User|Options", meta=(ClampMin="0.0", UIMin="0.0", UIMax="10.0"))
	float CameraZoomLagSpeed{5.f};

	/** Controls how quickly the camera reaches target rotation. Low values are slower (more lag), high values are faster (less lag), while zero is instant (no lag). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="User|Options", meta=(ClampMin="0.0", ClampMax="1000.0", UIMin="0.0", UIMax="20.0"))
	float CameraRotationLagSpeed{6.f};

	/** How quickly the camera rotates. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="User|Options", meta=(ClampMin="0.1", UIMin="0.1", UIMax="2.0"))
	float RotateSpeed{0.5f};

	/** How often the client player's location is sent to the server in FPS. Lower for better performance. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="User", AdvancedDisplay, meta=(ClampMin="0.1", ClampMax="60.0", UIMin="1.0", UIMax="30.0", Units="times"))
	float SendTransform_TimerFreq{3.f};

	/** How often the locally controlled pawn's height is updated in FPS. Lower for better performance. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="User", AdvancedDisplay, meta=(ClampMin="0.1", ClampMax="60.0", UIMin="1.0", UIMax="30.0", Units="times"))
	float TraceForHeight_TimerFreq{5.f};

	/** The channel used to set the height. Should check against the ground or building floors so we don't fall through */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="User|Options")
	TEnumAsByte<ECollisionChannel> TerrainHeightTraceChannel{ECC_Visibility};
#pragma endregion

protected:
	/** Draws a number of debug visualizations for testing during PIE. Draws a trace to the ground. Draws Rotations (Control=Yellow, Actor=Orange, SpringArm=Red). Draws markers at the camera target (in green) and the lagged position (in yellow). A line is drawn between the two locations, in green. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="User|Info")
	bool bDrawDebugMarkers{false};

};
