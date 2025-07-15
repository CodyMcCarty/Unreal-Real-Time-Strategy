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

/* Notes on blocking: Does the spring arm have any collision? Does the camera? Does the ActorRoot
 * __Kenshi__
 * Person and Cam can only go so far. Cam can look (spring arm) past end. No clipping in terrain, but in buildings and most anything else.
 * Actor=(WorldEnd). SpringArm=(None). Cam=(terrain)
 * __Tropico__
 * Can not look up clamped at -15. cam not clip through terrain, but buildigns. actor stoped at world end but can look past
 * Actor=(WorldEnd). SpringArm=(none). Cam=(terrain)
 * __UE Setup for collision__
 * Channel | Ignore | Overlap | Block
 * Visibility: Ignore=(Pawn)
 * Camera
 *
 * 3rdPrsFloor(Default)	Visibility=?		| Camera=?
 * Land(BlockAll)		Visibility=Block	| Camera=Block	[b]
 * NewFloor(BlkAlDy)	Visibility=Block	| Camera=Block	[b]
 * PhysicsActor			Visibility=Block	| Camera=Block	[b]
 * Projectile			Visibility=Block	| Camera=Block	[b]
 * Vehicle				Visibility=Block	| Camera=Block	[i]
 * Roof(Custom)			Visibility=Ignore	| Camera=Ignore	[i]
 * Pawn:				Visibility=Ignore	| Camera=Block	[D]
 * CharacterMesh:		Visibility=Ignore	| Camera=Block	[D]
 * OverlapOnlyPawn		Visibility=Block	| Camera=Ignore	[D]
 * InvisibleWall		Visibility=Ignore	| Camera=Block	[D]
 * Trigger				Visibility=Ignore	| Camera=Ovrlap	[D]
 * Ragdoll				Visibility=Ignore	| Camera=Block	[D]
 * UI					Visibility=Block	| Camera=Ovrlap	[D]
 */

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
	void TimerLoop_TraceForCameraHeight();
	void TimerLoop_SendSimpleRepMovement();
	void Move(const FInputActionInstance& InputActionInstance);
	void Zoom(const FInputActionInstance& InputActionInstance);
	void RotateStarted(const FInputActionInstance& InputActionInstance);
	void Rotate(const FInputActionInstance& InputActionInstance);
	void EnableRotateStarted(const FInputActionInstance& InputActionInstance);
	void EnableRotateEnded(const FInputActionInstance& InputActionInstance);

	/** Is the target transform of locally controlled pawns OR the last received transform for other net roles. Visible in PIE for debugging. */
	UPROPERTY(VisibleInstanceOnly, Category="User|Info", ReplicatedUsing=OnRep_SimpleRepMovement)
	FSimpleRepMovement SimpleRepMovement;

	UFUNCTION()
	void OnRep_SimpleRepMovement(const FSimpleRepMovement& OldSimpleRepMovement);

	UFUNCTION(Server, Unreliable)
	void Server_SetSimpleRepMovementState(const FSimpleRepMovement NewSimpleRepMovement);

	UPROPERTY(EditAnywhere, BlueprintReadWrite) // todo: does this a uprop()?
	TObjectPtr<USpringArmComponent> SpringArmComp;
	
	FTimerHandle SendSimpleRepMovement_TimerHandle;
	FTimerHandle TraceForCameraHeight_TimerHandle;
	FIntPoint MousePosSnapshot;

	/** Draws a number of debug visualizations for testing during PIE. Draws a trace for the ground. Draws Rotations (Control=Yellow, Actor=Orange, SpringArm=Red). Draws markers at the camera target (in green) and the lagged position (in yellow). A line is drawn between the two locations, in green. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="User|Info")
	bool bDrawDebugMarkers{false};

	/** This value is calculated. Based on Zoom, Min&MaxZoom, and Min&MaxMoveSpeed. Zoomed out goes faster. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="User|Info", meta=(Units="cm/s"))
	float MoveSpeedCalculated{800.f};

	/** Zoom. The length of the SpringArm after interpolation. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="User|Info", meta=(Units="cm"))
	float ZoomArmLength{800.f};

	// todo: consider having a BPConfig data asset.
#pragma region BPConfig

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

	/** Controls how quickly the camera reaches target position. Low values are slower (more lag), high values are faster (less lag), while zero is instant (no lag). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="User|Options", meta=(ClampMin="0.0", ClampMax="1000.0", UIMin = "0.0", UIMax = "50.0"))
	float CameraLagSpeed{5.f};

	/** Zoom adjusts speed. This is the speed when zoomed all the way in. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="User|Options", meta=(ClampMin="50.0", UIMin="200.0", UIMax="4000.0", Units="cm/s"))
	float MinMoveSpeed{1100.f};

	/** Zoom adjusts speed. This is the speed when zoomed all the way out. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="User|Options", meta=(ClampMin="50.0", UIMin="1000.0", UIMax="30000.0", Units="cm/s"))
	float MaxMoveSpeed{15000.f};

	/** This also affects speed */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="User|Options", meta=(ClampMin="50.0", UIMin="50.0", UIMax="500.0", Units="cm"))
	float MinZoom{200.f};

	/** This also affects speed */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="User|Options", meta=(ClampMin="50.0", UIMin="1000.0", UIMax="15000.0", Units="cm"))
	float MaxZoom{5000.f};

	/** Controls how quickly the camera reaches target position. Low values are slower (more lag), high values are faster (less lag), while zero is instant (no lag). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="User|Options", meta=(ClampMin="0.0", UIMin="0.0", UIMax="10.0"))
	float CameraZoomLagSpeed{5.f};

	/** Controls how quickly the camera reaches target position. Low values are slower (more lag), high values are faster (less lag), while zero is instant (no lag). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="User|Options", meta=(ClampMin="0.0", ClampMax="1000.0", UIMin = "0.0", UIMax = "50.0"))
	float CameraRotationLagSpeed{7.f};

	/** How often the client player's location is sent to the server in FPS. Lower for better performance. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="User", AdvancedDisplay, meta=(ClampMin=0.1f, ClampMax=60.f, UIMin=1.f, UIMax=30.f, Units="times"))
	float SendTransform_TimerFreq{3.f};

	/** How often the locally controlled pawn's height is updated in FPS. Lower for better performance. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="User", AdvancedDisplay, meta=(ClampMin=0.1f, ClampMax=60.f, UIMin=1.f, UIMax=30.f, Units="times"))
	float TraceForCameraHeight_TimerFreq{5.f};

	/** The channel used to set the height. Should check against the ground or building floors so we don't fall through */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="User|Options")
	TEnumAsByte<ECollisionChannel> TerrainHeightTraceChannel{ECC_Visibility};
#pragma endregion

protected:
	bool IsCamClippingGround(FHitResult& OutHit) const;
	FRotator FindCamLookAtRot(const FHitResult& InDesiredCamHit) const;
};
