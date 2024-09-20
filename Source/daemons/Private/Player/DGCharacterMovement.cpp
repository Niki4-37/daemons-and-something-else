// For Daemons and something else videogame purpose only

#include "Player/DGCharacterMovement.h"
#include "GameFramework/Character.h"
#include "Components/CapsuleComponent.h"

void UDGCharacterMovement::UpdateCharacterStateBeforeMovement(float DeltaSeconds) 
{
    const bool bIsSprinting{IsCustomMovementMode(CMOVE_Sprint)};
    const bool bIsCrawling{IsCustomMovementMode(CMOVE_Crawling)};

	GEngine->AddOnScreenDebugMessage(-1, 0.f, FColor::Orange, UEnum::GetValueAsString(TEnumAsByte<ECustomMovementMode>(CustomMovementMode)));
	GEngine->AddOnScreenDebugMessage(-1, 0.f, FColor::Orange, UEnum::GetValueAsString(TEnumAsByte<EMovementMode>(MovementMode)));

	FFindFloorResult FloorResult;
	FindFloor(UpdatedComponent->GetComponentLocation(), FloorResult, false);

    if (bWantsToSprint                              //
        && !Velocity.IsNearlyZero()                 //
        && !bIsCrouching							//
		&& !bIsCrawling								//
		&& !bWantsToCrawling						//
		&& !bWantsToCrouch	//
		&& !IsFalling())
    {
        SetMovementMode(MOVE_Custom, CMOVE_Sprint);
    }

	GEngine->AddOnScreenDebugMessage(-1, 0.f, FColor::Orange, FString::Printf(TEXT("%s"), FloorResult.bWalkableFloor ? TEXT("Has floor") : TEXT("No floor")));

    if (!bWantsToSprint && bIsSprinting)
    {
        SetMovementMode(DefaultLandMovementMode);
    }

	if (bIsSprinting && !FloorResult.bWalkableFloor)
    {
		SetMovementMode(MOVE_Falling);
	}

	if (bWantsToCrouch								//
		&& CanCrouchInCurrentState())				//
	{
		EnterCrouching(DeltaSeconds);
	}
	if ((!bWantsToCrouch || !CanCrouchInCurrentState())				//
		&& bIsCrouching
		&& !FMath::IsNearlyZero(TrackedProcessTime)					//
		&& (MovementMode != MOVE_Custom))							//
	{
		ExitCrouching(DeltaSeconds);
	}

	if (FFindFloorResult FloorHit; bWantsToCrawling && CanCrawling(FloorHit))
	{
		EnterCrawling(DeltaSeconds);
	}
	if (bIsCrawling && !bWantsToCrawling)
	{
		ExitCrawling(DeltaSeconds);
	}
}

float UDGCharacterMovement::GetMaxSpeed() const
{
    const float MaxSpeed = Super::GetMaxSpeed();
	if (MovementMode != MOVE_Custom)
	{
		return MaxSpeed;
	}

	switch (CustomMovementMode)
	{
		case CMOVE_Sprint : return SprintSpeed;
		case CMOVE_Crawling : return CrawlingSpeed;
		default: return MaxSpeed;
	}
}

void UDGCharacterMovement::InitializeComponent() 
{
    Super::InitializeComponent();

	if (CharacterOwner)
    {
		const auto DefaultCharacter = CharacterOwner->GetClass()->GetDefaultObject<ACharacter>();
		DefaultHalfHeight = DefaultCharacter 
			?
			DefaultCharacter->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight()
			:
			CharacterOwner->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight();
	}

	/* halfheight depends of condition */
	HalfHeightMap.Add(EStandCondition::STANDING, DefaultHalfHeight);
	HalfHeightMap.Add(EStandCondition::SITTING, GetCrouchedHalfHeight());
	HalfHeightMap.Add(EStandCondition::LYING, CrawlingHeight);
}

void UDGCharacterMovement::PhysCustom(float DeltaTime, int32 Iterations) 
{
    Super::PhysCustom(DeltaTime, Iterations);

	switch (CustomMovementMode)
	{
        case CMOVE_Sprint:
			PhysSprint(DeltaTime, Iterations);
			break;
		case CMOVE_Crawling:
			PhysCrawling(DeltaTime, Iterations);
			break;
		default:
			UE_LOG(LogTemp, Error, TEXT("Invalid Movement Mode"))
	}
}

bool UDGCharacterMovement::CanAttemptJump() const
{
    return true;
}

void UDGCharacterMovement::PhysSprint(float DeltaTime, int32 Iterations) 
{
	PhysWalking(DeltaTime, Iterations);
}

void UDGCharacterMovement::EnterCrouching(float DeltaSeconds) 
{
	if (!CharacterOwner || FMath::IsNearlyZero(SitdownProcessDuration)) return;

	GEngine->AddOnScreenDebugMessage(-1, 0.f, FColor::Orange, "Enter Crouching");

	SetMovementMode(MOVE_Walking);
	bIsCrouching = true;

	const float IncreaseTimeModifier = 1.f;
	ChangeCapsuleHalfHeight(GetCrouchedHalfHeight(), DeltaSeconds, IncreaseTimeModifier);
	if (FMath::IsNearlyEqual(GetCrouchedHalfHeight(), CharacterOwner->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight()))
    {
        Crouch(false);
		TrackedProcessTime = SitdownProcessDuration;
		SetStandCondition(EStandCondition::SITTING);
    }
}

void UDGCharacterMovement::ExitCrouching(float DeltaSeconds) 
{
	if (!CharacterOwner || HasTopObstacle(DefaultHalfHeight) || FMath::IsNearlyZero(SitdownProcessDuration)) return;

	GEngine->AddOnScreenDebugMessage(-1, 0.f, FColor::Orange, "Exit Crouching");

	bWantsToCrawling ? SetStandCondition(EStandCondition::LYING) : SetStandCondition(EStandCondition::STANDING);

	const float DecreaseTimeModifier = -1.f;
	ChangeCapsuleHalfHeight(GetCrouchedHalfHeight(), DeltaSeconds, DecreaseTimeModifier);

	if (FMath::IsNearlyEqual(GetHalfHeightByStandCondition(), CharacterOwner->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight()))
    {
        UnCrouch(false);
		TrackedProcessTime = 0.f;
		bIsCrouching = false;
		SetMovementMode(DefaultLandMovementMode);
    }    
}

void UDGCharacterMovement::EnterCrawling(float DeltaSeconds) 
{
	if (!CharacterOwner || FMath::IsNearlyZero(SitdownProcessDuration)) return;

	GEngine->AddOnScreenDebugMessage(-1, 0.f, FColor::Orange, "Enter Crawling");

	SetMovementMode(MOVE_Custom, CMOVE_Crawling);

	if (FMath::IsNearlyEqual(GetCrouchedHalfHeight(), CharacterOwner->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight()))
    {
        UnCrouch(false);
		TrackedProcessTime = 0.f;
		bIsCrouching = false;
    }
	
	const float IncreaseBuffTime = 1.f;
	ChangeCapsuleHalfHeight(CrawlingHeight, DeltaSeconds, IncreaseBuffTime);
	if (FMath::IsNearlyEqual(CrawlingHeight, CharacterOwner->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight()))
    {
		TrackedProcessTime = SitdownProcessDuration;
		SetStandCondition(EStandCondition::LYING);
    }
}

void UDGCharacterMovement::ExitCrawling(float DeltaSeconds) 
{
	if (!CharacterOwner || HasTopObstacle(GetCrouchedHalfHeight()) || FMath::IsNearlyZero(SitdownProcessDuration)) return;

	GEngine->AddOnScreenDebugMessage(-1, 0.f, FColor::Orange, "Exit Crawling");

	SetStandCondition(EStandCondition::SITTING);

	const float DecreaseBuffTime = -1.f;
	ChangeCapsuleHalfHeight(CrawlingHeight, DeltaSeconds, DecreaseBuffTime);
	if (FMath::IsNearlyEqual(GetHalfHeightByStandCondition(), CharacterOwner->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight()))
    {
		EnterCrouching(DeltaSeconds);
    }
}

bool UDGCharacterMovement::CanCrawling(FFindFloorResult& FloorResult) const
{
    if (!UpdatedComponent) return false;
    FindFloor(UpdatedComponent->GetComponentLocation(), FloorResult, false);
	return FloorResult.bWalkableFloor;
}

void UDGCharacterMovement::PhysCrawling(float DeltaTime, int32 Iterations) 
{
	PhysWalking(DeltaTime, Iterations);
}

bool UDGCharacterMovement::HasTopObstacle(float HeightToCheck)
{
    if (!CharacterOwner) return false;
    
    const float ComponentScale = CharacterOwner->GetCapsuleComponent()->GetShapeScale();
	const float OldUnscaledHalfHeight = CharacterOwner->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight();
	const float HalfHeightAdjust = HeightToCheck - OldUnscaledHalfHeight;
	
	if (FMath::IsNearlyZero(HalfHeightAdjust)) return false;
	
	const float ScaledHalfHeightAdjust = HalfHeightAdjust * ComponentScale;
    const FVector SweepStart = CharacterOwner->GetCapsuleComponent()->GetComponentLocation();
    const FVector SweepEnd = SweepStart + CharacterOwner->GetCapsuleComponent()->GetUpVector() * ScaledHalfHeightAdjust;
	
    const FCollisionShape SphereShape = FCollisionShape::MakeSphere(CharacterOwner->GetCapsuleComponent()->GetUnscaledCapsuleRadius());
    const ECollisionChannel CollisionChannel = CharacterOwner->GetCapsuleComponent()->GetCollisionObjectType();
    FCollisionQueryParams QueryParams;
    QueryParams.AddIgnoredActor(CharacterOwner);
    TArray<FHitResult> OutResults;
    GetWorld()->SweepMultiByChannel(OutResults, SweepStart, SweepEnd, FQuat::Identity, CollisionChannel, SphereShape, QueryParams);
    for (const FHitResult& OutResult : OutResults)
    {
        if (OutResult.bBlockingHit) return true;
    }

    return false;
}

void UDGCharacterMovement::ChangeCapsuleHalfHeight(float ShortHalfHeight, float DeltaSeconds, float Modifier) 
{
	const float Value = FMath::Clamp(TrackedProcessTime += DeltaSeconds * Modifier, 0.f, SitdownProcessDuration) / SitdownProcessDuration;
    const float HalfHeight = FMath::Lerp(GetHalfHeightByStandCondition(), ShortHalfHeight, Value);
    CharacterOwner->GetCapsuleComponent()->SetCapsuleHalfHeight(HalfHeight);
}

bool UDGCharacterMovement::IsCustomMovementMode(const ECustomMovementMode InCustomMovementMode) const
{
    return MovementMode == MOVE_Custom && CustomMovementMode == InCustomMovementMode;
}
