// Copyright Epic Games, Inc. All Rights Reserved.

#include "AstralPlagueCharacter.h"

#include "AbilitySystem/AstralAbilitySystemComponent.h"
#include "AbilitySystem/AstralPlayerController.h"
#include "AbilitySystem/AstralPlayerState.h"
#include "AbilitySystem/AstralStatsComponent.h"
#include "Engine/LocalPlayer.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/Controller.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"

DEFINE_LOG_CATEGORY(LogTemplateCharacter);

//////////////////////////////////////////////////////////////////////////
// AAstralPlagueCharacter

AAstralPlagueCharacter::AAstralPlagueCharacter()
{
	// Set size for collision capsule
	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.0f);
		
	// Don't rotate when the controller rotates. Let that just affect the camera.
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	// Configure character movement
	GetCharacterMovement()->bOrientRotationToMovement = true; // Character moves in the direction of input...	
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 500.0f, 0.0f); // ...at this rotation rate

	// Note: For faster iteration times these variables, and many more, can be tweaked in the Character Blueprint
	// instead of recompiling to adjust them
	GetCharacterMovement()->JumpZVelocity = 700.f;
	GetCharacterMovement()->AirControl = 0.35f;
	GetCharacterMovement()->MaxWalkSpeed = 500.f;
	GetCharacterMovement()->MinAnalogWalkSpeed = 20.f;
	GetCharacterMovement()->BrakingDecelerationWalking = 2000.f;
	GetCharacterMovement()->BrakingDecelerationFalling = 1500.0f;

	// Create a camera boom (pulls in towards the player if there is a collision)
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 400.0f; // The camera follows at this distance behind the character	
	CameraBoom->bUsePawnControlRotation = true; // Rotate the arm based on the controller

	// Create a follow camera
	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName); // Attach the camera to the end of the boom and let the boom adjust to match the controller orientation
	FollowCamera->bUsePawnControlRotation = false; // Camera does not rotate relative to arm

	// Note: The skeletal mesh and anim blueprint references on the Mesh component (inherited from Character) 
	// are set in the derived blueprint asset named ThirdPersonCharacter (to avoid direct content references in C++)
}

void AAstralPlagueCharacter::BeginPlay()
{
	// Call the base class  
	Super::BeginPlay();
}

//////////////////////////////////////////////////////////////////////////
// Input

void AAstralPlagueCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	// Add Input Mapping Context
	if (APlayerController* PlayerController = Cast<APlayerController>(GetController()))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PlayerController->GetLocalPlayer()))
		{
			Subsystem->AddMappingContext(DefaultMappingContext, 0);
		}
	}
	
	// Set up action bindings
	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent)) {
		
		// Jumping
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &ACharacter::Jump);
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &ACharacter::StopJumping);

		// Moving
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AAstralPlagueCharacter::Move);

		// Looking
		EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &AAstralPlagueCharacter::Look);
	}
	else
	{
		UE_LOG(LogTemplateCharacter, Error, TEXT("'%s' Failed to find an Enhanced Input component! This template is built to use the Enhanced Input system. If you intend to use the legacy system, then you will need to update this C++ file."), *GetNameSafe(this));
	}
}

void AAstralPlagueCharacter::Move(const FInputActionValue& Value)
{
	// input is a Vector2D
	FVector2D MovementVector = Value.Get<FVector2D>();

	if (Controller != nullptr)
	{
		// find out which way is forward
		const FRotator Rotation = Controller->GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);

		// get forward vector
		const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
	
		// get right vector 
		const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

		// add movement 
		AddMovementInput(ForwardDirection, MovementVector.Y);
		AddMovementInput(RightDirection, MovementVector.X);
	}
}

void AAstralPlagueCharacter::Look(const FInputActionValue& Value)
{
	// input is a Vector2D 
	FVector2D LookAxisVector = Value.Get<FVector2D>();

	if (Controller != nullptr)
	{
		// add yaw and pitch input to controller
		AddControllerYawInput(LookAxisVector.X);
		AddControllerPitchInput(LookAxisVector.Y);
	}
}

AAstralPlayerController* AAstralPlagueCharacter::GetAstralPlayerController() const
{
	return CastChecked<AAstralPlayerController>(Controller, ECastCheckedType::NullAllowed);
}

AAstralPlayerState* AAstralPlagueCharacter::GetAstralPlayerState() const
{
	return CastChecked<AAstralPlayerState>(GetPlayerState(), ECastCheckedType::NullAllowed);
}

UAstralAbilitySystemComponent* AAstralPlagueCharacter::GetAstralAbilitySystemComponent() const
{
	return Cast<UAstralAbilitySystemComponent>(GetAbilitySystemComponent());
}

UAbilitySystemComponent* AAstralPlagueCharacter::GetAbilitySystemComponent() const
{
	/*if (PawnEx == nullptr)
	{
		return nullptr;
	}*/

	//return PawnExtComponent->GetLyraAbilitySystemComponent(); This is the default Lyra implementation but I'm not entirely sure the purpose of the ext component and as such I don't want to implement it
	return GetAstralAbilitySystemComponent();
}

void AAstralPlagueCharacter::GetOwnedGameplayTags(FGameplayTagContainer& TagContainer) const
{
	if (const UAstralAbilitySystemComponent* AstralASC = GetAstralAbilitySystemComponent())
	{
		AstralASC->GetOwnedGameplayTags(TagContainer);
	}
}

bool AAstralPlagueCharacter::HasMatchingGameplayTag(FGameplayTag TagToCheck) const
{
	if (const UAstralAbilitySystemComponent* AstralASC = GetAstralAbilitySystemComponent())
	{
		return AstralASC->HasMatchingGameplayTag(TagToCheck);
	}

	return false;
}

bool AAstralPlagueCharacter::HasAllMatchingGameplayTags(const FGameplayTagContainer& TagContainer) const
{
	if (const UAstralAbilitySystemComponent* AstralASC = GetAstralAbilitySystemComponent())
	{
		return AstralASC->HasAllMatchingGameplayTags(TagContainer);
	}

	return false;
}

bool AAstralPlagueCharacter::HasAnyMatchingGameplayTags(const FGameplayTagContainer& TagContainer) const
{
	if (const UAstralAbilitySystemComponent* AstralASC = GetAstralAbilitySystemComponent())
	{
		return AstralASC->HasAnyMatchingGameplayTags(TagContainer);
	}

	return false;
}

void AAstralPlagueCharacter::OnAbilitySystemInitialized()
{
	UAstralAbilitySystemComponent* AstralASC = GetAstralAbilitySystemComponent();
	check(AstralASC);

	StatsComponent->InitializeWithAbilitySystem(AstralASC);

	//InitializeGameplayTags(); 
}

void AAstralPlagueCharacter::OnAbilitySystemUninitialized()
{
	StatsComponent->UninitializeFromAbilitySystem();
}

