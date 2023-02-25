// Fill out your copyright notice in the Description page of Project Settings.


#include "Menu.h"
#include "MultiplayerSessionsSubsystem.h"
#include "OnlineSubsystem.h"
#include "Components/Button.h"

void UMenu::SetupMenu(int32 NumberOfPublicConnections, FString TypeOfMatch, int32 MaxNumberOfSearchResults, FString PathOfLobby)
{
	AddToViewport();
	SetVisibility(ESlateVisibility::Visible);
	bIsFocusable = true;
	NumPublicConnections = NumberOfPublicConnections;
	MatchType = TypeOfMatch;
	MaxSearchResults = MaxNumberOfSearchResults;
	LobbyPath = FString::Printf(TEXT("%s?listen"), *PathOfLobby);

	UWorld* World = GetWorld();
	if (World)
	{
		APlayerController* PlayerController = World->GetFirstPlayerController();
		if (PlayerController)
		{
			FInputModeUIOnly InputModeData;
			InputModeData.SetWidgetToFocus(TakeWidget());
			InputModeData.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
			PlayerController->SetInputMode(InputModeData);
			PlayerController->SetShowMouseCursor(true);
		}
	}
	// Get access to the MultiplayerSessionsSubsystem
	UGameInstance* GameInstance = GetGameInstance();
	if (GameInstance)
	{
		MultiplayerSessionsSubsystem = GameInstance->GetSubsystem<UMultiplayerSessionsSubsystem>();
	}
	// Bind the callbacks to the custom delegates once the menu is set up
	if (MultiplayerSessionsSubsystem)
	{
		MultiplayerSessionsSubsystem->MultiplayerOnCreateSessionComplete.AddDynamic(this, &ThisClass::OnCreateSession);
		MultiplayerSessionsSubsystem->MultiplayerOnFindSessionsComplete.AddUObject(this, &ThisClass::OnFindSessions);
		MultiplayerSessionsSubsystem->MultiplayerOnJoinSessionComplete.AddUObject(this, &ThisClass::OnJoinSession);
		MultiplayerSessionsSubsystem->MultiplayerOnStartSessionComplete.AddDynamic(this, &ThisClass::OnStartSession);
		MultiplayerSessionsSubsystem->MultiplayerOnDestroySessionComplete.AddDynamic(this, &ThisClass::OnDestroySession);
	}
	FWorldDelegates::LevelRemovedFromWorld.AddUObject(this, &UMenu::OnLevelRemovedFromWorld);
	HostButton->bIsEnabled = false;
	//HostButton->
}

bool UMenu::Initialize()
{
	if (!Super::Initialize())
	{
		return false;
	}
	if (HostButton)
	{
		HostButton->OnClicked.AddDynamic(this, &ThisClass::HostButtonClicked);
	}
	if (JoinButton)
	{
		JoinButton->OnClicked.AddDynamic(this, &ThisClass::JoinButtonClicked);
	}
	return true;
}

void UMenu::HostButtonClicked()
{
	if (MultiplayerSessionsSubsystem)
	{
		HostButton->SetIsEnabled(false);
		MultiplayerSessionsSubsystem->CreateSession(NumPublicConnections, MatchType);
	}
}

void UMenu::JoinButtonClicked()
{
	if (MultiplayerSessionsSubsystem)
	{
		JoinButton->SetIsEnabled(false);
		MultiplayerSessionsSubsystem->FindSessions(MaxSearchResults);
	}
}

void UMenu::ShutOffMenu()
{
	RemoveFromParent();
	UWorld* World = GetWorld();
	if (World)
	{
		APlayerController* PlayerController = World->GetFirstPlayerController();
		if (PlayerController)
		{
			PlayerController->SetInputMode(FInputModeGameOnly());
			PlayerController->SetShowMouseCursor(false);
		}
	}
}

void UMenu::OnCreateSession(bool bWasSuccessful)
{
	return;
	// Print the session created result
	if (bWasSuccessful)
	{
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(
				-1,
				15.f,
				FColor::Blue,
				FString(TEXT("Created Successfully"))
			);
		}
		// Server Travel to the Lobby Level
		UWorld* World = GetWorld();
		if (World)
		{
			World->ServerTravel(LobbyPath);
		}
	}
	else
	{
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(
				-1,
				15.f,
				FColor::Red,
				FString(TEXT("Failed to create session!"))
			);
		}
		HostButton->SetIsEnabled(true);
	}
}

void UMenu::OnFindSessions(const TArray<FOnlineSessionSearchResult>& SessionResults, bool bWasSuccessful)
{
	return;
	if (MultiplayerSessionsSubsystem == nullptr)
	{
		return;
	}
	// Recover the button status
	if (!bWasSuccessful || SessionResults.Num() <= 0)
	{
		JoinButton->SetIsEnabled(true);
		return;
	}
	// Look through the search result and find the matching one, then join the session
	for (auto Result : SessionResults)
	{
		FString Id = Result.GetSessionIdStr();
		FString User = Result.Session.OwningUserName;

		// Check the match type
		// We get the <key> FName("MatchType") and put the <value> result into the FString variable
		FString SettingsValue;
		Result.Session.SessionSettings.Get(FName("MatchType"), SettingsValue);
		
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(
				-1,
				15.f,
				FColor::Cyan,
				FString::Printf(TEXT("Id: %s, User, %s"), *Id, *User)
			);

			// Check and print the MatchType result
			if (SettingsValue == MatchType)
			{
				GEngine->AddOnScreenDebugMessage(
					-1,
					15.f,
					FColor::Cyan,
					FString::Printf(TEXT("Joining Match Type is: %s"), *SettingsValue)
				);
				MultiplayerSessionsSubsystem->JoinSession(Result);
				return;
			}
		}
	}
}

void UMenu::OnJoinSession(EOnJoinSessionCompleteResult::Type Result)
{
	return;
	// Print the connected info and Client Travel to the new level
	if (Result == EOnJoinSessionCompleteResult::Success)
	{
		IOnlineSubsystem* Subsystem = IOnlineSubsystem::Get();
		if (Subsystem)
		{
			IOnlineSessionPtr OnlineSessionInterface = Subsystem->GetSessionInterface();
		
			// Join the session
			if (OnlineSessionInterface.IsValid())
			{
				FString Address;
				if (OnlineSessionInterface->GetResolvedConnectString(NAME_GameSession, Address))
				{
					if (GEngine)
					{
						GEngine->AddOnScreenDebugMessage(
							-1,
							15.f,
							FColor::Yellow,
							FString::Printf(TEXT("Connected Info, %s"), *Address)
						);
					}
					APlayerController* PlayerController = GetGameInstance()->GetFirstLocalPlayerController();
					if (PlayerController)
					{
						PlayerController->ClientTravel(Address, ETravelType::TRAVEL_Absolute);
					}
				}
			}
		}
	}
	// Print the error message
	else
	{
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(
				-1,
				15.f,
				FColor::Red,
				FString(TEXT("Failed to join the session"))
			);
		}
		JoinButton->SetIsEnabled(true);
	}
}

void UMenu::OnStartSession(bool bWasSuccessful)
{
	
}

void UMenu::OnDestroySession(bool bWasSuccessful)
{

}

void UMenu::OnLevelRemovedFromWorld(ULevel* InLevel, UWorld* InWorld)
{
	ShutOffMenu();
}


