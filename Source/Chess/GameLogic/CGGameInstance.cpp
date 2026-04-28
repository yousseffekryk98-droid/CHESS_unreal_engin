// Fill out your copyright notice in the Description page of Project Settings.


#include "GameLogic/CGGameInstance.h"
#include "Online/OnlineSessionNames.h"
#include "GameLogic/CGSettingsSave.h"
#include "Online/OnlineSessionNames.h"
#include "Kismet/GameplayStatics.h"
#include "Online/OnlineSessionNames.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Online/OnlineSessionNames.h"
#include "Online/OnlineSessionNames.h"
#include "Online/OnlineSessionNames.h"
#include "GameLogic/CGOnlineSession.h"
#include "Online/OnlineSessionNames.h"
#include "Blueprint/CGBPUtils.h"
#include "Online/OnlineSessionNames.h"

UCGGameInstance::UCGGameInstance(const FObjectInitializer& ObjectInitializer)
	:Super(ObjectInitializer)
{
	/** Bind function for CREATING a Session */
	OnCreateSessionCompleteDelegate = FOnCreateSessionCompleteDelegate::CreateUObject(this, &UCGGameInstance::OnCreateSessionComplete);
	OnStartSessionCompleteDelegate = FOnStartSessionCompleteDelegate::CreateUObject(this, &UCGGameInstance::OnStartOnlineGameComplete);
	/** Bind function for FINDING a Session */
	OnFindSessionsCompleteDelegate = FOnFindSessionsCompleteDelegate::CreateUObject(this, &UCGGameInstance::OnFindSessionsComplete);
	/** Bind function for JOINING a Session */
	OnJoinSessionCompleteDelegate = FOnJoinSessionCompleteDelegate::CreateUObject(this, &UCGGameInstance::OnJoinSessionComplete);
	/** Bind function for DESTROYING a Session */
	OnDestroySessionCompleteDelegate = FOnDestroySessionCompleteDelegate::CreateUObject(this, &UCGGameInstance::OnDestroySessionComplete);
}

void UCGGameInstance::Init()
{
	Super::Init();
	LoadCfg();
}

void UCGGameInstance::SaveCfg()
{
	if (Settings)
	{
		FString SaveSlotName = TEXT("SaveSettings");
		int32 Slot = 0;
		UGameplayStatics::SaveGameToSlot(Settings, SaveSlotName, Slot);
	}
}

bool UCGGameInstance::LoadCfg()
{
	FString SaveSlotName = TEXT("SaveSettings");
	int32 Slot = 0;
	Settings = Cast<UCGSettingsSave>(UGameplayStatics::LoadGameFromSlot(SaveSlotName, Slot));
	if (!Settings)
	{
		Settings = Cast<UCGSettingsSave>(UGameplayStatics::CreateSaveGameObject(UCGSettingsSave::StaticClass()));
		//get user name for default name
		Settings->PlayerName = FText::FromString(UKismetSystemLibrary::GetPlatformUserName());
		Settings->LastIp = UCGBPUtils::GetLocalIP();
		Settings->EnginePath = UCGBPUtils::TryFindStockfishPath();
		return false;
	}
	return true;
}

void UCGGameInstance::JoinIP(const FString& Ip)
{
	if (Settings)
	{
		Settings->LastIp = Ip;
		SaveCfg();
	}
	Exec(GetWorld(), *FString::Printf(TEXT("open %s"), *Ip));
}

const FName UCGGameInstance::GetMyName() const
{ 
	if (Settings)
	{
		return FName(Settings->PlayerName.ToString());//search is not working so it doesn't matter anyway...
	}
	return FName(TEXT("Unnamed"));
}

TSharedPtr<const FUniqueNetId> UCGGameInstance::GetMyId() const
{
	return UGameplayStatics::GetGameInstance(GetWorld())->GetFirstGamePlayer()->GetPreferredUniqueNetId().GetUniqueNetId();
}

TSubclassOf<UOnlineSession> UCGGameInstance::GetOnlineSessionClass()
{
	return UCGOnlineSession::StaticClass();
}

//https://unreal.gg-labs.com/wiki-archives/networking/how-to-use-sessions-in-c++
bool UCGGameInstance::Host(const FString& iFen, bool iIsLan)
{
	if (Settings)
	{
		Settings->bIsLan = iIsLan;
		SaveCfg();
	}
	CurrentFen = iFen;
	return HostSession(GetMyId(), GetMyName(), iIsLan, true, 2);
}

bool UCGGameInstance::HostSession(TSharedPtr<const FUniqueNetId> iUserId, const FName& iSessionName, bool iIsLAN, bool iIsPresence, int32 iMaxNumPlayers)
{
	IOnlineSubsystem* const onlineSub = IOnlineSubsystem::Get();
	if (onlineSub)
	{
		IOnlineSessionPtr sessions = onlineSub->GetSessionInterface();
		if (sessions.IsValid() && iUserId.IsValid())
		{
			SessionSettings = MakeShareable(new FOnlineSessionSettings());
			SessionSettings->bIsLANMatch = iIsLAN;
			SessionSettings->bUsesPresence = iIsPresence;
			SessionSettings->NumPublicConnections = iMaxNumPlayers;
			SessionSettings->NumPrivateConnections = 0;
			SessionSettings->bAllowInvites = true;
			SessionSettings->bAllowJoinInProgress = true;
			SessionSettings->bShouldAdvertise = true;
			SessionSettings->bAllowJoinViaPresence = true;
			SessionSettings->bAllowJoinViaPresenceFriendsOnly = false;
			SessionSettings->Set(SETTING_MAPNAME, FString("Game"), EOnlineDataAdvertisementType::ViaOnlineService);
			OnCreateSessionCompleteDelegateHandle = sessions->AddOnCreateSessionCompleteDelegate_Handle(OnCreateSessionCompleteDelegate);
			return sessions->CreateSession(*iUserId, iSessionName, *SessionSettings);
		}
	}
	else
	{
		GEngine->AddOnScreenDebugMessage(-1, 10.f, FColor::Red, TEXT("No OnlineSubsytem found!"));
	}
	return false;
}

void UCGGameInstance::OnCreateSessionComplete(FName iSessionName, bool iWasSuccessful)
{
	GEngine->AddOnScreenDebugMessage(-1, 10.f, FColor::Red, FString::Printf(TEXT("OnCreateSessionComplete %s, %d"), *iSessionName.ToString(), iWasSuccessful));
	IOnlineSubsystem* onlineSub = IOnlineSubsystem::Get();
	if (onlineSub)
	{
		IOnlineSessionPtr sessions = onlineSub->GetSessionInterface();
		if (sessions.IsValid())
		{
			sessions->ClearOnCreateSessionCompleteDelegate_Handle(OnCreateSessionCompleteDelegateHandle);
			if (iWasSuccessful)
			{
				OnStartSessionCompleteDelegateHandle = sessions->AddOnStartSessionCompleteDelegate_Handle(OnStartSessionCompleteDelegate);
				sessions->StartSession(iSessionName);
			}
			else
			{
				sessions->DestroySession(iSessionName);
			}
		}
	}
}

void UCGGameInstance::OnStartOnlineGameComplete(FName iSessionName, bool iWasSuccessful)
{
	GEngine->AddOnScreenDebugMessage(-1, 10.f, FColor::Red, FString::Printf(TEXT("OnStartSessionComplete %s, %d"), *iSessionName.ToString(), iWasSuccessful));
	IOnlineSubsystem* onlineSub = IOnlineSubsystem::Get();
	if (onlineSub)
	{
		IOnlineSessionPtr sessions = onlineSub->GetSessionInterface();
		if (sessions.IsValid())
		{
			sessions->ClearOnStartSessionCompleteDelegate_Handle(OnStartSessionCompleteDelegateHandle);
		}
	}
	if (iWasSuccessful)
	{
		UGameplayStatics::OpenLevel(GetWorld(), "Game", true, FString::Printf(TEXT("listen?Fen=%s?"), *CurrentFen));
	}
}

void UCGGameInstance::Search()
{
	FindSessions(GetMyId(), true, false);
}

void UCGGameInstance::FindSessions(TSharedPtr<const FUniqueNetId> iUserId, bool iIsLAN, bool iIsPresence)
{
	IOnlineSubsystem* onlineSub = IOnlineSubsystem::Get();
	if (onlineSub)
	{
		IOnlineSessionPtr sessions = onlineSub->GetSessionInterface();
		if (sessions.IsValid() && iUserId.IsValid())
		{
			SessionSearch = MakeShareable(new FOnlineSessionSearch());
			SessionSearch->bIsLanQuery = iIsLAN;
			SessionSearch->MaxSearchResults = 20;
			SessionSearch->PingBucketSize = 50;
			if (iIsPresence)
			{
				SessionSearch->QuerySettings.Set(SEARCH_PRESENCE, iIsPresence, EOnlineComparisonOp::Equals);
			}
			TSharedRef<FOnlineSessionSearch> SearchSettingsRef = SessionSearch.ToSharedRef();
			OnFindSessionsCompleteDelegateHandle = sessions->AddOnFindSessionsCompleteDelegate_Handle(OnFindSessionsCompleteDelegate);
			sessions->FindSessions(*iUserId, SearchSettingsRef);
		}
	}
	else
	{
		OnFindSessionsComplete(false);
	}
}

void UCGGameInstance::OnFindSessionsComplete(bool iWasSuccessful)
{
	GEngine->AddOnScreenDebugMessage(-1, 10.f, FColor::Red, FString::Printf(TEXT("OFindSessionsComplete bSuccess: %d"), iWasSuccessful));
	TArray<FJoinableGame> results;
	IOnlineSubsystem* const onlineSub = IOnlineSubsystem::Get();
	if (onlineSub)
	{
		IOnlineSessionPtr sessions = onlineSub->GetSessionInterface();
		if (sessions.IsValid())
		{
			sessions->ClearOnFindSessionsCompleteDelegate_Handle(OnFindSessionsCompleteDelegateHandle);
			GEngine->AddOnScreenDebugMessage(-1, 10.f, FColor::Red, FString::Printf(TEXT("Num Search Results: %d"), SessionSearch->SearchResults.Num()));
			if (SessionSearch->SearchResults.Num() > 0)
			{
				for (FOnlineSessionSearchResult& res : SessionSearch->SearchResults)
				{
					//TODO: finish this if we want the search to work
					results.Emplace( "", res);
				}
			}
		}
	}
	SearchComplete(results, iWasSuccessful);
}

void UCGGameInstance::Join()
{
	FOnlineSessionSearchResult searchResult;
	if (SessionSearch && SessionSearch->SearchResults.Num() > 0)
	{
		for (int32 i = 0; i < SessionSearch->SearchResults.Num(); i++)
		{
			if (SessionSearch->SearchResults[i].Session.OwningUserId != GetMyId())
			{
				searchResult = SessionSearch->SearchResults[i];
				JoinSessionInternal(GetMyId(), NAME_GameSession, searchResult);
				break;
			}
		}
	}
}

bool UCGGameInstance::JoinSessionInternal(TSharedPtr<const FUniqueNetId> iUserId, FName iSessionName, const FOnlineSessionSearchResult& iSearchResult)
{
	bool success = false;
	IOnlineSubsystem* onlineSub = IOnlineSubsystem::Get();
	if (onlineSub)
	{
		IOnlineSessionPtr sessions = onlineSub->GetSessionInterface();
		if (sessions.IsValid() && iUserId.IsValid())
		{
			OnJoinSessionCompleteDelegateHandle = sessions->AddOnJoinSessionCompleteDelegate_Handle(OnJoinSessionCompleteDelegate);
			success = sessions->JoinSession(*iUserId, iSessionName, iSearchResult);
		}
	}
	return success;
}

void UCGGameInstance::OnJoinSessionComplete(FName iSessionName, EOnJoinSessionCompleteResult::Type iResult)
{
	GEngine->AddOnScreenDebugMessage(-1, 10.f, FColor::Red, FString::Printf(TEXT("OnJoinSessionComplete %s, %d"), *iSessionName.ToString(), static_cast<int32>(iResult)));
	IOnlineSubsystem* onlineSub = IOnlineSubsystem::Get();
	if (onlineSub)
	{
		IOnlineSessionPtr sessions = onlineSub->GetSessionInterface();

		if (sessions.IsValid())
		{
			sessions->ClearOnJoinSessionCompleteDelegate_Handle(OnJoinSessionCompleteDelegateHandle);
			APlayerController* const pc = GetFirstLocalPlayerController();
			FString travelURL;
			if (pc && sessions->GetResolvedConnectString(iSessionName, travelURL))
			{
				pc->ClientTravel(travelURL, ETravelType::TRAVEL_Absolute);
			}
		}
	}
}

void UCGGameInstance::DestroySession()
{
	IOnlineSubsystem* onlineSub = IOnlineSubsystem::Get();
	if (onlineSub)
	{
		IOnlineSessionPtr sessions = onlineSub->GetSessionInterface();
		if (sessions.IsValid())
		{
			sessions->AddOnDestroySessionCompleteDelegate_Handle(OnDestroySessionCompleteDelegate);
			sessions->DestroySession(GetMyName());
		}
	}
}

void UCGGameInstance::OnDestroySessionComplete(FName iSessionName, bool iWasSuccessful)
{
	GEngine->AddOnScreenDebugMessage(-1, 10.f, FColor::Red, FString::Printf(TEXT("OnDestroySessionComplete %s, %d"), *iSessionName.ToString(), iWasSuccessful));
	IOnlineSubsystem* onlineSub = IOnlineSubsystem::Get();
	if (onlineSub)
	{
		IOnlineSessionPtr sessions = onlineSub->GetSessionInterface();
		if (sessions.IsValid())
		{
			sessions->ClearOnDestroySessionCompleteDelegate_Handle(OnDestroySessionCompleteDelegateHandle);
			//back to the menu
			UGameplayStatics::OpenLevel(GetWorld(), "Game", true);
		}
	}
}
