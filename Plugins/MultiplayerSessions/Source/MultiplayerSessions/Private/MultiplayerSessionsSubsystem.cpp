// Fill out your copyright notice in the Description page of Project Settings.


#include "MultiplayerSessionsSubsystem.h"
#include "OnlineSubsystem.h"
#include "Kismet/GameplayStatics.h"

UMultiplayerSessionsSubsystem::UMultiplayerSessionsSubsystem():
	CreateSessionCompleteDelegate(FOnCreateSessionCompleteDelegate::CreateUObject(this, &ThisClass::OnCreateSessionComplete)),
	FindSessionsCompleteDelegate(FOnFindSessionsCompleteDelegate::CreateUObject(this, &ThisClass::OnFindSessionsComplete)),
	JoinSessionCompleteDelegate(FOnJoinSessionCompleteDelegate::CreateUObject(this, &ThisClass::OnJoinSessionComplete)),
	StartSessionCompleteDelegate(FOnStartSessionCompleteDelegate::CreateUObject(this, &ThisClass::OnStartSessionComplete)),
	DestroySessionCompleteDelegate(FOnDestroySessionCompleteDelegate::CreateUObject(this, &ThisClass::OnDestroySessionComplete))
{
	IOnlineSubsystem* Subsystem = IOnlineSubsystem::Get();
	if (Subsystem)
	{
		OnlineSessionInterface = Subsystem->GetSessionInterface();
	}
}

void UMultiplayerSessionsSubsystem::CreateSession(int32 NumPublicConnections, FString MatchType)
{
	UE_LOG(LogTemp, Warning, TEXT("Called init"));
	IOnlineSubsystem* Subsystem = IOnlineSubsystem::Get();
	if (Subsystem)
	{
		OnlineSessionInterface = Subsystem->GetSessionInterface();
	}
	if (!OnlineSessionInterface.IsValid())
	{
		return;
	}
	// Save the NumPublicConnections and MatchType for CreateSessionOnDestroyed
	LastNumPublicConnections = NumPublicConnections;
	LastMatchType = MatchType;
	
	// Remove the old session
	if (OnlineSessionInterface->GetNamedSession(ShooterSession))
	{
		bCreateSessionOnDestroy = true;
		DestroySession();
		return;
	}

	// Add the delegate into the delegate list and store the handle so we can remove the delegate later
	CreateSessionCompleteDelegateHandle = OnlineSessionInterface->AddOnCreateSessionCompleteDelegate_Handle(CreateSessionCompleteDelegate);

	// Create a new Session
	LastSessionSettings = MakeShareable(new FOnlineSessionSettings());
	LastSessionSettings->bIsLANMatch = IOnlineSubsystem::Get()->GetSubsystemName() == "NULL" ? true : false;	// Set up a session over the Internet or Local
	LastSessionSettings->NumPublicConnections = NumPublicConnections;	// Allow the number of players in the game
	LastSessionSettings->bAllowJoinInProgress = true;					// Allow players to join in when the program is running
	LastSessionSettings->bAllowJoinViaPresence = true;					// Steam's presence--searching for players in his region of the world
	LastSessionSettings->bShouldAdvertise = true;						// Allow other players to find the session by the advertise
	LastSessionSettings->bUsesPresence = true;							// Also like bAllowJoinViaPresence
	LastSessionSettings->bUseLobbiesIfAvailable = true;
	LastSessionSettings->BuildUniqueId = 1;
	
	// Set up a match type
	LastSessionSettings->Set(FName("MatchType"), MatchType, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
	
	const ULocalPlayer* LocalPlayer = GetWorld()->GetFirstLocalPlayerFromController();
	// Check the session, if created in failure, then remove the delegate from the delegate list ('OnCreateSessionComplete' won't be called)
	// and we broadcast the custom delegate.
	// These operations will also be done when we created the session successfully, but here we do is just for 'early time'
	UE_LOG(LogTemp, Warning, TEXT("qwq Local player: %d"), LocalPlayer);
	if (!OnlineSessionInterface->CreateSession(0, ShooterSession, *LastSessionSettings))
	{
		OnlineSessionInterface->ClearOnCreateSessionCompleteDelegate_Handle(CreateSessionCompleteDelegateHandle);
		// Broadcast our own custom delegate
		MultiplayerOnCreateSessionComplete.Broadcast(false);
	}
}

void UMultiplayerSessionsSubsystem::FindSessions(int32 MaxSearchResults)
{
	// Search for game sessions
	if (!OnlineSessionInterface.IsValid())
	{
		return;
	}

	// Add the delegate into the delegate list
	FindSessionsCompleteDelegateHandle = OnlineSessionInterface->AddOnFindSessionsCompleteDelegate_Handle(FindSessionsCompleteDelegate);

	// Search for sessions
	LastSessionSearch = MakeShareable(new FOnlineSessionSearch());
	LastSessionSearch->bIsLanQuery = IOnlineSubsystem::Get()->GetSubsystemName() == "NULL" ? true : false;;
	LastSessionSearch->MaxSearchResults = MaxSearchResults;
	LastSessionSearch->QuerySettings.Set(SEARCH_PRESENCE, true, EOnlineComparisonOp::Equals);
	
	const ULocalPlayer* LocalPlayer = GetWorld()->GetFirstLocalPlayerFromController();
	if (!OnlineSessionInterface->FindSessions(*LocalPlayer->GetPreferredUniqueNetId(),LastSessionSearch.ToSharedRef()))
	{
		OnlineSessionInterface->ClearOnFindSessionsCompleteDelegate_Handle(FindSessionsCompleteDelegateHandle);
		// If we failed to find the session, then we put an empty SessionSearchResult array
		MultiplayerOnFindSessionsComplete.Broadcast(TArray<FOnlineSessionSearchResult>(), false);
	}
}

void UMultiplayerSessionsSubsystem::JoinSession(const FOnlineSessionSearchResult& SessionResult)
{
	if (!OnlineSessionInterface.IsValid())
	{
		MultiplayerOnJoinSessionComplete.Broadcast(EOnJoinSessionCompleteResult::UnknownError);
		return;
	}
	// Add the delegate to the delegate list
	JoinSessionCompleteDelegateHandle = OnlineSessionInterface->AddOnJoinSessionCompleteDelegate_Handle(JoinSessionCompleteDelegate);

	// Join the game session
	const ULocalPlayer* LocalPlayer = GetWorld()->GetFirstLocalPlayerFromController();
	for (int i = 0; i < LastSessionSearch->SearchResults.Num(); i++)
	{
		OnlineSessionInterface->JoinSession(*LocalPlayer->GetPreferredUniqueNetId(), ShooterSession, SessionResult);
		/*
	if (!OnlineSessionInterface->JoinSession(*LocalPlayer->GetPreferredUniqueNetId(), NAME_GameSession, SessionResult))
	{
		OnlineSessionInterface->ClearOnJoinSessionCompleteDelegate_Handle(JoinSessionCompleteDelegateHandle);
		MultiplayerOnJoinSessionComplete.Broadcast(EOnJoinSessionCompleteResult::UnknownError);
	}*/
	}
}

void UMultiplayerSessionsSubsystem::StartSession()
{
	
}

void UMultiplayerSessionsSubsystem::DestroySession()
{
	if (!OnlineSessionInterface.IsValid())
	{
		MultiplayerOnDestroySessionComplete.Broadcast(false);
		return;
	}
	// Add the delegate to the delegate list
	DestroySessionCompleteDelegateHandle = OnlineSessionInterface->AddOnDestroySessionCompleteDelegate_Handle(DestroySessionCompleteDelegate);
	// Destroy the session
	if (!OnlineSessionInterface->DestroySession(ShooterSession))
	{
		OnlineSessionInterface->ClearOnDestroySessionCompleteDelegate_Handle(DestroySessionCompleteDelegateHandle);
		MultiplayerOnDestroySessionComplete.Broadcast(false);
	}
}

void UMultiplayerSessionsSubsystem::OnCreateSessionComplete(FName SessionName, bool bWasSuccessful)
{
	if (!OnlineSessionInterface.IsValid())
	{
		return;
	}
	// Remove the delegate from the delegate list
	OnlineSessionInterface->ClearOnCreateSessionCompleteDelegate_Handle(CreateSessionCompleteDelegateHandle);
	
	// Broadcast our own custom delegate
	MultiplayerOnCreateSessionComplete.Broadcast(bWasSuccessful);
	UE_LOG(LogTemp, Warning, TEXT("Session Creation: %d"), bWasSuccessful);
	if (bWasSuccessful)
	{
		UGameplayStatics::OpenLevel(GetWorld(), FName("/Game/Maps/Lobby"), true, "?listen");
	}
	//OnlineSessionInterface->DestroySession(SessionName);
}

void UMultiplayerSessionsSubsystem::OnFindSessionsComplete(bool bWasSuccessful)
{
	if (!OnlineSessionInterface.IsValid())
	{
		return;
	}
	UE_LOG(LogTemp,Warning, TEXT("Found sessions with result: %d, num: %d"), bWasSuccessful, LastSessionSearch->SearchResults.Num());
	// Remove the delegate from the delegate list
	OnlineSessionInterface->ClearOnCreateSessionCompleteDelegate_Handle(CreateSessionCompleteDelegateHandle);

	if (LastSessionSearch->SearchResults.Num() <= 0)
	{
		MultiplayerOnFindSessionsComplete.Broadcast(TArray<FOnlineSessionSearchResult>(), false);
		return;
	}
	// Broadcast our own custom delegate
	MultiplayerOnFindSessionsComplete.Broadcast(LastSessionSearch->SearchResults, bWasSuccessful);
	
	for (FOnlineSessionSearchResult Session : LastSessionSearch->SearchResults)
	{
		if (Session.Session.OwningUserName.Contains("pc-maksymets"))
		{
			JoinSessionCompleteDelegateHandle = OnlineSessionInterface->AddOnJoinSessionCompleteDelegate_Handle(JoinSessionCompleteDelegate);
			ULocalPlayer* Player = GetWorld()->GetFirstLocalPlayerFromController();
			OnlineSessionInterface->JoinSession(*Player->GetPreferredUniqueNetId(), ShooterSession, Session);
			break;
		}
		else//Ignore
			UE_LOG(LogTemp, Warning, TEXT("Ignoring session: %s"), *Session.Session.OwningUserName);
	}
		/*
		TArray<FOnlineSessionSearchResult> Sessions = LastSessionSearch->SearchResults;
		//for (FOnlineSessionSearchResult Session : Sessions)
		{
			FOnlineSessionSearchResult Sess = Sessions[i];
			FOnlineSession Session = Sess.Session;
			UE_LOG(LogTemp, Warning, TEXT("qwr\nNumber: %d\nPing: %d\nIs Valid: %d\nNetId: %s\nOwningUserName: %s\nIsDedicated: %d"), i, Sess.PingInMs, Sess.IsValid(), *Session.OwningUserId->ToString(), *Session.OwningUserName, Session.SessionSettings.bIsDedicated);
		}


		if(0 && LastSessionSearch->SearchResults[i].Session.OwningUserName.Contains(FString("pc-mak")))
		{
			OnlineSessionInterface->JoinSession(*Player->GetPreferredUniqueNetId(), ShooterSession, LastSessionSearch->SearchResults[i]);
		}
		*/
	
}

void UMultiplayerSessionsSubsystem::OnJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result)
{
	if (!OnlineSessionInterface.IsValid())
	{
		return;
	}
	// Remove the delegate from the delegate list
	OnlineSessionInterface->ClearOnJoinSessionCompleteDelegate_Handle(JoinSessionCompleteDelegateHandle);

	// Broadcast our own custom delegate
	MultiplayerOnJoinSessionComplete.Broadcast(Result);
	UE_LOG(LogTemp, Warning, TEXT("Joining with result: %d"), static_cast<int32>(Result));
	if (Result == EOnJoinSessionCompleteResult::Type::Success)
	{
		FString url;
		OnlineSessionInterface->GetResolvedConnectString(SessionName, url);
		UGameplayStatics::OpenLevel(GetWorld(), FName(url));
	}
}

void UMultiplayerSessionsSubsystem::OnStartSessionComplete(FName SessionName, bool bWasSuccessful)
{
	
}

void UMultiplayerSessionsSubsystem::OnDestroySessionComplete(FName SessionName, bool bWasSuccessful)
{
	if (!OnlineSessionInterface.IsValid())
	{
		return;
	}
	// Remove the delegate from the delegate list
	OnlineSessionInterface->ClearOnDestroySessionCompleteDelegate_Handle(DestroySessionCompleteDelegateHandle);

	// If needs create a new session after destroyed
	if (bWasSuccessful && bCreateSessionOnDestroy)
	{
		bCreateSessionOnDestroy = false;
		CreateSession(LastNumPublicConnections, LastMatchType);
	}
	// Broadcast our own custom delegate
	MultiplayerOnDestroySessionComplete.Broadcast(bWasSuccessful);
}

