// Copyright Cody McCarty.

#include "StratPlayerCameraPawn.h"

#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "KismetTraceUtils.h"
#include "SandCoreLogToolsBPLibrary.h"
#include "GameFramework/SpringArmComponent.h"
#include "Kismet/KismetMathLibrary.h"
#include "Net/UnrealNetwork.h"

DEFINE_LOG_CATEGORY(LogGame);

namespace
{
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

	SimpleRepMovement.Location = GetActorLocation();
	SimpleRepMovement.Yaw = GetActorRotation().Yaw;

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
		GetWorldTimerManager().SetTimer(TraceForCameraHeight_TimerHandle, this, &ThisClass::TimerLoop_TraceForCameraHeight, 1 / TraceForCameraHeight_TimerFreq, true);

		SetInputMode_RTSStyle(Cast<APlayerController>(Controller));
	}

	Super::NotifyControllerChanged(); //~Super calls BP handler, and PreviousController = Controller;
}

void AStratPlayerCameraPawn::OnRep_Controller()
{
	Super::OnRep_Controller();

	GetWorldTimerManager().SetTimer(SendSimpleRepMovement_TimerHandle, this, &ThisClass::TimerLoop_SendSimpleRepMovement, 1 / SendTransform_TimerFreq, true);
}

UE_DISABLE_OPTIMIZATION

bool AStratPlayerCameraPawn::IsCamClippingGround(FHitResult& OutHit) const
{
	const FVector CamLoc = SpringArmComp->UnfixedCameraPosition;
	const bool bHit = GetWorld()->SweepSingleByChannel
	(
		OutHit,
		CamLoc + FVector(0.f, 0.f, 1000.f),
		CamLoc - FVector(0.f, 0.f, 1000.f),
		FQuat::Identity, TerrainHeightTraceChannel,
		FCollisionShape::MakeSphere(100.f),
		FCollisionQueryParams(SCENE_QUERY_STAT(CameraPawn), false, this)
	);

	return bHit && OutHit.Location.Z > CamLoc.Z;
}

FRotator AStratPlayerCameraPawn::FindCamLookAtRot(const FHitResult& InDesiredCamHit) const
{
	FRotator ResultRot = Controller->GetControlRotation().GetNormalized();
	const FVector CamLoc = SpringArmComp->UnfixedCameraPosition;
	const FVector SafeCamLoc(CamLoc.X, CamLoc.Y, InDesiredCamHit.Location.Z);
	const FRotator LookAtRot = FRotationMatrix::MakeFromX(GetActorLocation() - SafeCamLoc).Rotator();
	ResultRot.Pitch = LookAtRot.Pitch;

	return ResultRot;
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
			SimpleRepMovement.Location += Velocity;
		}

		//~ Zoom ~
		{
			SpringArmComp->TargetArmLength = FMath::FInterpTo(SpringArmComp->TargetArmLength, ZoomArmLength, DeltaTime, CameraZoomLagSpeed);
		}

		//~ Rotation ~ (Pitch controls the SpringArmComp. Yaw controls the Actor.)
		{
			FHitResult DesiredCamHit;
			if (IsCamClippingGround(DesiredCamHit))
			{
				const FVector CurrLoc = GetActorLocation();
				const FVector CamLoc = SpringArmComp->UnfixedCameraPosition;
				const float NewHeight = DesiredCamHit.Location.Z - (CamLoc.Z - CurrLoc.Z);
				SetActorLocation(FVector(CurrLoc.X, CurrLoc.Y, NewHeight));
			}

			const FRotator ControlRot = Controller->GetControlRotation().GetNormalized();
			SimpleRepMovement.Yaw = ControlRot.Yaw;
			DrawDebugCone(GetWorld(), GetActorLocation(), GetControlRotation().Vector(), 175.f, 0.7f, 0.f, 8, FColor::Yellow);

			const FRotator SpringArmRot = SpringArmComp->GetRelativeRotation();
			const FRotator CurrentRot(SpringArmRot.Pitch, GetActorRotation().Yaw, 0.f);
			const FRotator TargetRot(FMath::QInterpTo(FQuat(CurrentRot), FQuat(ControlRot), DeltaTime, CameraRotationLagSpeed));

			SetActorRotation(FRotator(0.f, TargetRot.Yaw, 0.f));
			SpringArmComp->SetRelativeRotation(FRotator(TargetRot.Pitch, 0.f, 0.f));
		}

		if (GetLocalRole() == ROLE_AutonomousProxy)
		{
		}
	}
	else
	{
		const FRotator ProxyRot(0.f, SimpleRepMovement.Yaw, 0.f);
		SetActorRotation(ProxyRot);
	}

	// todo: FMath::ComputeSlideVector
	//~ Apply Movement ~
	const FVector ActorLocation = GetActorLocation();
	const FVector TargetLocation = FMath::VInterpTo(ActorLocation, SimpleRepMovement.Location, DeltaTime, CameraLagSpeed);
	SetActorLocation(TargetLocation);

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	if (bDrawDebugMarkers)
	{
		if (const UWorld* World = GetWorld())
		{
			DrawDebugSphere(World, SimpleRepMovement.Location, 5.f, 8, FColor::Green);
			DrawDebugDirectionalArrow(World, ActorLocation, SimpleRepMovement.Location, 7.5f, FColor::Green);
			DrawDebugCone(World, ActorLocation, GetActorForwardVector(), 175.f, 0.7f, 0.f, 8, FColor::Yellow);
		}
	}
#endif
}

UE_ENABLE_OPTIMIZATION

void AStratPlayerCameraPawn::TimerLoop_TraceForCameraHeight()
{
	const UWorld* World = GetWorld();
	if (!World) { return; }

	TArray<FHitResult> HitResults;
	const FVector Dist(0.f, 0.f, 10'000.f);
	const FVector Start = SimpleRepMovement.Location + Dist;
	const FVector End = SimpleRepMovement.Location - Dist;
	constexpr float Radius = 75.f;
	FCollisionQueryParams QueryParams = FCollisionQueryParams::DefaultQueryParam; // 
	QueryParams.AddIgnoredActor(this);
	QueryParams.bTraceComplex = false;
	const bool bHit = World->SweepMultiByChannel
	(HitResults,
		Start,
		End,
		FQuat::Identity,
		TerrainHeightTraceChannel,
		FCollisionShape::MakeSphere(Radius),
		FCollisionQueryParams(SCENE_QUERY_STAT(CameraPawn), false, this)
	);

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	if (bDrawDebugMarkers)
	{
		DrawDebugSphereTraceMulti(World, Start, End, Radius, EDrawDebugTrace::ForDuration, bHit, HitResults, FLinearColor::Red, FLinearColor::Green, 1 / TraceForCameraHeight_TimerFreq);
	}
#endif

	//~ a blocking hit (if found) will be the last element of the array. the results are [0] is top floor [last] is terrain.
	//~ todo: have floor system with terrain block, and floors overlap.
	if (!HitResults.IsEmpty())
	{
		const float GroundZ = HitResults[0].Location.Z;
		SimpleRepMovement.Location.Z = GroundZ;
	}
}

void AStratPlayerCameraPawn::TimerLoop_SendSimpleRepMovement()
{
	SimpleRepMovement.ServerFrame++;
	Server_SetSimpleRepMovementState(SimpleRepMovement);
}

void AStratPlayerCameraPawn::Move(const FInputActionInstance& InputActionInstance)
{
	const FVector2D Direction = InputActionInstance.GetValue().Get<FVector2D>().GetSafeNormal();
	AddMovementInput(GetActorForwardVector(), Direction.Y);
	AddMovementInput(GetActorRightVector(), Direction.X);
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
	const FVector2D RotateValue = InputActionInstance.GetValue().Get<FVector2D>();
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
