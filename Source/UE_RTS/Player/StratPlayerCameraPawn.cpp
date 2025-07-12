// Copyright Cody McCarty.

#include "StratPlayerCameraPawn.h"

#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "KismetTraceUtils.h"
#include "GameFramework/SpringArmComponent.h"
#include "Net/UnrealNetwork.h"

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
		Result.Pitch = FMath::Clamp(Result.Pitch, -85.f, 10.f);
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
	SpringArmComp->TargetArmLength = TargetArmLength;
	SpringArmComp->SetRelativeRotation(FRotator(-45.f, 0.f, 0.f));

	const float ArmLengthNormal = GetZoomAlpha(TargetArmLength, MinZoom, MaxZoom);
	MoveSpeed = FMath::Lerp(MinMoveSpeed, MaxMoveSpeed, ArmLengthNormal);
}

void AStratPlayerCameraPawn::GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION(AStratPlayerCameraPawn, SimpleReplicatedMovement, COND_SkipOwner);
}

void AStratPlayerCameraPawn::PostInitializeComponents()
{
	Super::PostInitializeComponents();
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

	SimpleReplicatedMovement.Location = GetActorLocation();
	SimpleReplicatedMovement.Yaw = GetActorRotation().Yaw;
	
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

void AStratPlayerCameraPawn::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (IsLocallyControlled())
	{
		//~ Movement
		const FVector InputVector = ConsumeMovementInputVector();
		const FVector Velocity = InputVector * MoveSpeed * DeltaTime;
		SimpleReplicatedMovement.Location += Velocity;

		//~ Zoom
		SpringArmComp->TargetArmLength = FMath::FInterpTo(SpringArmComp->TargetArmLength, TargetArmLength, DeltaTime, CameraZoomLagSpeed);

		//~ Rotation
		const FRotator ControlRot = ClampPitch(GetControlRotation().GetNormalized());
		SimpleReplicatedMovement.Yaw = ControlRot.Yaw;
		Controller->SetControlRotation(ControlRot.GetDenormalized());

		const FRotator CurrentRot(SpringArmComp->GetRelativeRotation().Pitch, GetActorRotation().Yaw, 0.f);
		const FRotator TargetRot(FMath::QInterpTo(FQuat(CurrentRot), FQuat(ControlRot), DeltaTime, CameraRotationLagSpeed));

		SpringArmComp->SetRelativeRotation(FRotator(TargetRot.Pitch, 0.f, 0.f));
		SetActorRotation(FRotator(0.f, TargetRot.Yaw, 0.f));;

		if (GetLocalRole() == ROLE_AutonomousProxy)
		{
		}
	}
	else
	{
		const FRotator ProxyRot(0.f, SimpleReplicatedMovement.Yaw, 0.f);
		SetActorRotation(ProxyRot);
	}

	FVector ActorLocation = GetActorLocation();
	FVector TargetLocation = FMath::VInterpTo(ActorLocation, SimpleReplicatedMovement.Location, DeltaTime, CameraLagSpeed);
	SetActorLocation(TargetLocation);

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	if (bDrawDebugMarkers)
	{
		if (const UWorld* World = GetWorld())
		{
			DrawDebugSphere(World, ActorLocation, 5.f, 8, FColor::Green);
			DrawDebugSphere(World, SimpleReplicatedMovement.Location, 5.f, 8, FColor::Yellow);

			const FVector ToOrigin = ActorLocation - SimpleReplicatedMovement.Location;
			DrawDebugDirectionalArrow(World, SimpleReplicatedMovement.Location, SimpleReplicatedMovement.Location + ToOrigin * 0.5f, 7.5f, FColor::Green);
			DrawDebugDirectionalArrow(World, SimpleReplicatedMovement.Location + ToOrigin * 0.5f, ActorLocation, 7.5f, FColor::Green);

			DrawDebugCone(World, ActorLocation, GetControlRotation().Vector(), 125.f, 0.7f, 0.f, 8, FColor::Yellow);
			DrawDebugCone(World, ActorLocation, GetActorForwardVector(), 175.f, 0.7f, 0.f, 8, FColor::Orange);
			DrawDebugCone(World, ActorLocation, SpringArmComp->GetForwardVector(), 225.f, 0.7f, 0.f, 8, FColor::Red);
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
	const FVector Start = SimpleReplicatedMovement.Location + Dist;
	const FVector End = SimpleReplicatedMovement.Location - Dist;
	constexpr float Radius = 50.f;
	FCollisionQueryParams QueryParams = FCollisionQueryParams::DefaultQueryParam;
	QueryParams.AddIgnoredActor(this);
	QueryParams.bTraceComplex = false;
	const bool bHit = World->SweepMultiByChannel(HitResults, Start, End, FQuat::Identity, TerrainHeightTraceChannel, FCollisionShape::MakeSphere(Radius), QueryParams);

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	if (bDrawDebugMarkers)
	{
		DrawDebugSphereTraceMulti(World, Start, End, Radius, EDrawDebugTrace::ForDuration, bHit, HitResults, FLinearColor::Red, FLinearColor::Green, 1 / TraceForCameraHeight_TimerFreq);
	}
#endif

	//~ a blocking hit (if found) will be the last element of the array. the results are [0] is top floor [last] is terrain.
	//~ todo: have floor system. I could have terrain block, and floors overlap.
	if (!HitResults.IsEmpty())
	{
		const float GroundZ = HitResults[0].Location.Z;
		SimpleReplicatedMovement.Location.Z = GroundZ;
	}
}

void AStratPlayerCameraPawn::TimerLoop_SendSimpleRepMovement()
{
	SimpleReplicatedMovement.ServerFrame++;
	Server_SetSimpleReplicatedMovementState(SimpleReplicatedMovement);
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

	TargetArmLength += ZoomDelta * ZoomValue;
	TargetArmLength = FMath::Clamp(TargetArmLength, MinZoom, MaxZoom);

	const float ArmLengthNormal = GetZoomAlpha(TargetArmLength, MinZoom, MaxZoom);
	MoveSpeed = FMath::Lerp(MinMoveSpeed, MaxMoveSpeed, ArmLengthNormal);
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

void AStratPlayerCameraPawn::OnRep_SimpleReplicatedMovement(const FSimpleRepMovement& OldSimpleRepMovement)
{
	check(!IsLocallyControlled());
	if (OldSimpleRepMovement.ServerFrame > SimpleReplicatedMovement.ServerFrame)
	{
		SimpleReplicatedMovement = OldSimpleRepMovement;
	}
}

// todo: can\should I pass a ref?
void AStratPlayerCameraPawn::Server_SetSimpleReplicatedMovementState_Implementation(const FSimpleRepMovement NewSimpleRepMovement)
{
	if (NewSimpleRepMovement.ServerFrame > SimpleReplicatedMovement.ServerFrame)
	{
		SimpleReplicatedMovement = NewSimpleRepMovement;
	}
}
