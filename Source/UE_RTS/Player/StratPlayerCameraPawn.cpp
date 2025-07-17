// Copyright Cody McCarty.

#include "StratPlayerCameraPawn.h"

#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "KismetTraceUtils.h"
#include "SandCoreLogToolsBPLibrary.h"
#include "GameFramework/SpringArmComponent.h"
#include "Net/UnrealNetwork.h"

DEFINE_LOG_CATEGORY(LogGame);

namespace
{
	/** Just basing this on 16bit height maps. Consider moving this to a GameConstants.h/namespace GameConstants{} or UDeveloperSettings or find this num in UE. */
	inline constexpr float MapHalfHeight = 32'500.f;

	float GetZoomAlpha(const float TargetArmLength, const float MinZoom, const float MaxZoom)
	{
		return (TargetArmLength - MinZoom) / (MaxZoom - MinZoom);
	}

	void SetInputMode_RTSStyle(APlayerController* PC)
	{
		check(PC)
		FInputModeGameAndUI GameAndUIInputMode;
		GameAndUIInputMode.SetHideCursorDuringCapture(false);
		GameAndUIInputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		PC->SetInputMode(GameAndUIInputMode);
		PC->SetShowMouseCursor(true);
	}

	FRotator ClampPitch(const FRotator& Rot)
	{
		FRotator Result = Rot;
		Result.Pitch = FMath::Clamp(Result.Pitch, -85.f, 20.f); // <- normalized. UnNormal Rot would be
		return Result;
	}

	bool GetMousePos(const APlayerController* PC, FIntPoint& OutMousePos)
	{
		if (PC)
		{
			if (const ULocalPlayer* Player = PC->GetLocalPlayer())
			{
				if (const UGameViewportClient* ViewportClient = Player->ViewportClient)
				{
					if (FViewport* Viewport = ViewportClient->Viewport)
					{
						Viewport->GetMousePos(OutMousePos);
						if (OutMousePos.X >= 0 && OutMousePos.Y >= 0)
						{
							return true;
						}
					}
				}
			}
		}
		return false;
	}

	bool SetMousePos(const APlayerController* PC, const FIntPoint& MousePos)
	{
		if (PC)
		{
			if (const ULocalPlayer* Player = PC->GetLocalPlayer())
			{
				if (const UGameViewportClient* ViewportClient = Player->ViewportClient)
				{
					if (FViewport* Viewport = ViewportClient->Viewport)
					{
						Viewport->SetMouse(MousePos.X, MousePos.Y);
						return true;
					}
				}
			}
		}
		return false;
	}
}

AStratPlayerCameraPawn::AStratPlayerCameraPawn()
{
	PrimaryActorTick.bCanEverTick = true;
	SetReplicatingMovement(false);

	RootComponent = CreateDefaultSubobject<USceneComponent>("SceneRoot");
	SpringArmComp = CreateDefaultSubobject<USpringArmComponent>("SpringArmComp");
	SpringArmComp->SetupAttachment(RootComponent);

	SpringArmComp->bDoCollisionTest = false;
	SpringArmComp->CameraLagSpeed = CameraLagSpeed;
	SpringArmComp->CameraRotationLagSpeed = CameraRotationLagSpeed;
	SpringArmComp->bEnableCameraLag = false;
	SpringArmComp->bEnableCameraRotationLag = false;
	SpringArmComp->TargetArmLength = ZoomArmLength;
	SpringArmComp->SetRelativeRotation(FRotator(-45.f, 0.f, 0.f));

	const float ArmLengthNormal = GetZoomAlpha(ZoomArmLength, MinZoom, MaxZoom);
	MoveSpeedCalculated = FMath::Lerp(MinMoveSpeed, MaxMoveSpeed, ArmLengthNormal);
	MapBounds.bIsValid = true;

#if (UE_BUILD_SHIPPING)
	bDrawDebugMarkers = false;
#endif
}

void AStratPlayerCameraPawn::GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION(AStratPlayerCameraPawn, SimpleRepMovement, COND_SkipOwner);
}

void AStratPlayerCameraPawn::SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	const ULocalPlayer* LocalPlayer = CastChecked<APlayerController>(Controller)->GetLocalPlayer();
	UEnhancedInputLocalPlayerSubsystem* EnhancedInputSS = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(LocalPlayer);
	check(EnhancedInputSS);

	if (ensure(CameraMovementInputMapping))
	{
		EnhancedInputSS->AddMappingContext(CameraMovementInputMapping, 1);
	}

	UEnhancedInputComponent* EnhancedInputComp = CastChecked<UEnhancedInputComponent>(PlayerInputComponent);

	EnhancedInputComp->BindAction(Input_Move, ETriggerEvent::Triggered, this, &ThisClass::Move);
	EnhancedInputComp->BindAction(Input_Zoom, ETriggerEvent::Triggered, this, &ThisClass::Zoom);
	EnhancedInputComp->BindAction(Input_Rotate, ETriggerEvent::Started, this, &ThisClass::RotateStarted);
	EnhancedInputComp->BindAction(Input_Rotate, ETriggerEvent::Triggered, this, &ThisClass::Rotate);
	EnhancedInputComp->BindAction(Input_EnableRotate, ETriggerEvent::Started, this, &ThisClass::EnableRotateStarted);
	EnhancedInputComp->BindAction(Input_EnableRotate, ETriggerEvent::Canceled, this, &ThisClass::EnableRotateEnded);
	EnhancedInputComp->BindAction(Input_EnableRotate, ETriggerEvent::Completed, this, &ThisClass::EnableRotateEnded);
}

void AStratPlayerCameraPawn::BeginPlay()
{
	Super::BeginPlay();

	{
		const FVector ActorLocation = GetActorLocation();
		TargetMoveLoc = ActorLocation;
		SimpleRepMovement.Location = ActorLocation;
		SimpleRepMovement.Yaw = GetActorRotation().Yaw;
	}

	if (Controller)
	{
		const FRotator ControlRot = GetControlRotation();
		const FRotator SpringArmRot = SpringArmComp->GetRelativeRotation();
		const FRotator WithSpringArmPitch(SpringArmRot.GetDenormalized().Pitch, ControlRot.Yaw, ControlRot.Roll);
		Controller->SetControlRotation(WithSpringArmPitch);
	}
}

void AStratPlayerCameraPawn::NotifyControllerChanged()
{
	if (IsLocallyControlled())
	{
		GetWorldTimerManager().SetTimer(TraceForHeight_TimerHandle, this, &ThisClass::TimerLoop_TraceForHeight, 1 / TraceForHeight_TimerFreq, true);

		SetInputMode_RTSStyle(Cast<APlayerController>(Controller));
	}

	Super::NotifyControllerChanged(); //~Super calls BP handler, and PreviousController = Controller;
}

void AStratPlayerCameraPawn::OnRep_Controller()
{
	Super::OnRep_Controller();

	GetWorldTimerManager().SetTimer(SendSimpleRepMovement_TimerHandle, this, &ThisClass::TimerLoop_ServerSetSimpleRepMovement, 1 / SendTransform_TimerFreq, true);
}

void AStratPlayerCameraPawn::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (IsLocallyControlled())
	{
		//~ Movement ~
		{
			const FVector InputVector = ConsumeMovementInputVector();
			const FVector Velocity = InputVector * MoveSpeedCalculated * DeltaTime;
			TargetMoveLoc += Velocity;
			TargetMoveLoc.Z = GroundHit.Location.Z;
		}

		//~ Zoom ~
		{
			SpringArmComp->TargetArmLength = FMath::FInterpTo(SpringArmComp->TargetArmLength, ZoomArmLength, DeltaTime, CameraZoomLagSpeed);
		}

		//~ Rotation ~ (Pitch controls the SpringArmComp. Yaw controls the Actor.)
		{
			const FRotator CurrentRot(SpringArmComp->GetRelativeRotation().Pitch, GetActorRotation().Yaw, 0.f);
			const FRotator ControlRot = Controller->GetControlRotation().GetNormalized();

			const FRotator TargetRot(FMath::QInterpTo(FQuat(CurrentRot), FQuat(ControlRot), DeltaTime, CameraRotationLagSpeed));

			SetActorRotation(FRotator(0.f, TargetRot.Yaw, 0.f));
			SpringArmComp->SetRelativeRotation(FRotator(TargetRot.Pitch, 0.f, 0.f));
			SimpleRepMovement.Yaw = ControlRot.Yaw;
		}

		//~ Apply Movement ~
		{
			const FVector UnfixedCamLoc = GroundHit.Location + (-SpringArmComp->GetForwardVector() * ZoomArmLength);
			FHitResult CamHit;
			const bool bCamHit = TraceForCamCollision(CamHit, UnfixedCamLoc);
			const bool bIsCamClippingGround = bCamHit && CamHit.Location.Z > UnfixedCamLoc.Z;
			if (bIsCamClippingGround)
			{
				const float OffsetForCamClipping = CamHit.Location.Z - UnfixedCamLoc.Z;
				TargetMoveLoc.Z += OffsetForCamClipping;
			}

			const FVector ActorLoc = GetActorLocation();
			FVector NewLoc = FMath::VInterpTo(ActorLoc, TargetMoveLoc, DeltaTime, CameraLagSpeed);

			if (bIsCamClippingGround)
			{
				NewLoc.Z = TargetMoveLoc.Z;
			}

			SetActorLocation(NewLoc);
			SimpleRepMovement.Location = FVector(TargetMoveLoc.X, TargetMoveLoc.Y, GroundHit.Location.Z);
		}
	}
	else
	{
		//~ Apply network Proxy Movement ~
		TargetMoveLoc = SimpleRepMovement.Location;
		const FVector ActorLoc = GetActorLocation();
		const FVector NewLoc = FMath::VInterpTo(ActorLoc, TargetMoveLoc, DeltaTime, CameraLagSpeed);
		SetActorLocation(NewLoc);

		const FRotator ProxyRot(0.f, SimpleRepMovement.Yaw, 0.f);
		SetActorRotation(ProxyRot);
	}

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	if (bDrawDebugMarkers)
	{
		if (const UWorld* World = GetWorld())
		{
			const FVector ActorLoc = GetActorLocation();
			DrawDebugSphere(World, TargetMoveLoc, 5.f, 8, FColor::Green);
			DrawDebugDirectionalArrow(World, ActorLoc, TargetMoveLoc, 7.5f, FColor::Green);
			DrawDebugCone(World, ActorLoc, GetActorForwardVector(), 175.f, 0.7f, 0.f, 8, FColor::Yellow);
		}
	}
#endif
}

void AStratPlayerCameraPawn::TimerLoop_TraceForHeight()
{
	const UWorld* World = GetWorld();
	if (!World) { return; }

	TArray<FHitResult> HitResults;
	const FVector Start = GetActorLocation() + FVector::UpVector * MapHalfHeight;
	const FVector End = GetActorLocation() - FVector::UpVector * MapHalfHeight;
	constexpr float Radius = 75.f;
	const bool bHit = World->SweepMultiByChannel
	(HitResults,
		Start,
		End,
		FQuat::Identity,
		TerrainHeightTraceChannel,
		FCollisionShape::MakeSphere(Radius),
		FCollisionQueryParams(SCENE_QUERY_STAT(CameraPawn_TraceForHeight), false, this)
	);

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	if (bDrawDebugMarkers)
	{
		DrawDebugSphereTraceMulti(World, Start, End, Radius, EDrawDebugTrace::ForDuration, bHit, HitResults, FLinearColor::Red, FLinearColor::Green, 1 / TraceForHeight_TimerFreq);
	}
#endif

	//~ a blocking hit (if found) will be the last element of the array. the results are [0] is top floor [last] is terrain.
	// todo: have floor system with terrain block, and floors overlap.
	if (!HitResults.IsEmpty())
	{
		GroundHit = HitResults[0];
	}
}

void AStratPlayerCameraPawn::TimerLoop_ServerSetSimpleRepMovement()
{
	SimpleRepMovement.ServerFrame++;
	Server_SetSimpleRepMovementState(SimpleRepMovement);
}

// todo: consider using a sphere collision component. Could I remove the FloorTraceChannel var?
bool AStratPlayerCameraPawn::TraceForCamCollision(FHitResult& OutHit, const FVector& CamLoc) const // to make static: const AActor* WorldContextObj, ECollisionChannel TerrainHeightTraceChannel, const bool DrawDebug
{
	const UWorld* World = GetWorld();
	if (!World) { return false; }

	const FVector Start = CamLoc + FVector::UpVector * MapHalfHeight;
	constexpr float CamCollisionRadius = 100.f;
	const FVector End = CamLoc - FVector::UpVector * CamCollisionRadius * 3;
	const bool bHit = World->SweepSingleByChannel
	(
		OutHit,
		Start,
		End,
		FQuat::Identity,
		TerrainHeightTraceChannel,
		FCollisionShape::MakeSphere(CamCollisionRadius),
		FCollisionQueryParams(SCENE_QUERY_STAT(CameraPawn_TraceForCamHeight), false, this)
	);

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	if (bDrawDebugMarkers)
	{
		DrawDebugSphereTraceSingle(World, Start, End, CamCollisionRadius, EDrawDebugTrace::ForOneFrame, bHit, OutHit, FLinearColor::Red, FLinearColor::Green, 1.f);
	}
#endif

	return bHit;
}

void AStratPlayerCameraPawn::Move(const FInputActionInstance& InputActionInstance)
{
	const FVector2D Direction = InputActionInstance.GetValue().Get<FVector2D>().GetSafeNormal();
	AddMovementInput(GetActorForwardVector(), Direction.Y);
	AddMovementInput(GetActorRightVector(), Direction.X);

	if (MapBounds.bIsValid && !MapBounds.IsInside(FVector2D(TargetMoveLoc.X, TargetMoveLoc.Y)))
	{
		TargetMoveLoc.X = FMath::Clamp(TargetMoveLoc.X, MapBounds.Min.X, MapBounds.Max.X);
		TargetMoveLoc.Y = FMath::Clamp(TargetMoveLoc.Y, MapBounds.Min.Y, MapBounds.Max.Y);
	}
}

void AStratPlayerCameraPawn::Zoom(const FInputActionInstance& InputActionInstance)
{
	const float ZoomValue = InputActionInstance.GetValue().Get<float>();
	const float ZoomDelta = FMath::Max(SpringArmComp->TargetArmLength / UE_GOLDEN_RATIO, 50.f);

	ZoomArmLength += ZoomDelta * ZoomValue;
	ZoomArmLength = FMath::Clamp(ZoomArmLength, MinZoom, MaxZoom);

	const float ArmLengthNormal = GetZoomAlpha(ZoomArmLength, MinZoom, MaxZoom);
	MoveSpeedCalculated = FMath::Lerp(MinMoveSpeed, MaxMoveSpeed, ArmLengthNormal);
}

void AStratPlayerCameraPawn::RotateStarted(const FInputActionInstance& InputActionInstance)
{
	APlayerController* PC = CastChecked<APlayerController>(Controller);
	if (PC->bShowMouseCursor)
	{
		PC->SetShowMouseCursor(false);
		GetMousePos(PC, MousePosSnapshot);
		PC->SetInputMode(FInputModeGameOnly());
	}
}

void AStratPlayerCameraPawn::Rotate(const FInputActionInstance& InputActionInstance)
{
	FVector2D RotateValue = InputActionInstance.GetValue().Get<FVector2D>();
	RotateValue *= RotateSpeed;
	AddControllerYawInput(RotateValue.X);
	AddControllerPitchInput(RotateValue.Y);

	const FRotator ControlRot = ClampPitch(Controller->GetControlRotation().GetNormalized());
	Controller->SetControlRotation(ControlRot.GetDenormalized());
}

void AStratPlayerCameraPawn::EnableRotateStarted(const FInputActionInstance& InputActionInstance)
{
	APlayerController* PC = CastChecked<APlayerController>(Controller);
	if (PC->bShowMouseCursor)
	{
		PC->SetShowMouseCursor(false);
		GetMousePos(PC, MousePosSnapshot);
		PC->SetInputMode(FInputModeGameOnly());
	}
}

void AStratPlayerCameraPawn::EnableRotateEnded(const FInputActionInstance& InputActionInstance)
{
	APlayerController* PC = CastChecked<APlayerController>(Controller);
	SetInputMode_RTSStyle(PC);
	SetMousePos(PC, MousePosSnapshot);
}

void AStratPlayerCameraPawn::OnRep_SimpleRepMovement(const FSimpleRepMovement& OldSimpleRepMovement)
{
	check(!IsLocallyControlled());
	if (OldSimpleRepMovement.ServerFrame > SimpleRepMovement.ServerFrame)
	{
		SimpleRepMovement = OldSimpleRepMovement;
		INFO_CLOG(bDrawDebugMarkers, LogGame, Log, TEXT("Rejected out of date OnRep_RepMovement."))
	}
}

void AStratPlayerCameraPawn::Server_SetSimpleRepMovementState_Implementation(const FSimpleRepMovement NewSimpleRepMovement)
{
	if (NewSimpleRepMovement.ServerFrame > SimpleRepMovement.ServerFrame)
	{
		SimpleRepMovement = NewSimpleRepMovement;
	}
	else
	{
		INFO_CLOG(bDrawDebugMarkers, LogGame, Log, TEXT("Rejected out of date SetNew_RepMovement."))
	}
}
