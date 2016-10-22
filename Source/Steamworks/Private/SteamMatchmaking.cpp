//Copyright 2016 davevillz, https://github.com/davevill

#include "SteamworksPrivatePCH.h"
#include "SteamMatchmaking.h"
#include "SteamworksManager.h"
#include "SteamworksGameSession.h"
#include "Runtime/Core/Public/Misc/Base64.h"
#include "UniqueNetIdSteam.h"
#include "SteamLobby.h"




class FSteamMatchmakingImpl
{
private:

	TWeakObjectPtr<ASteamMatchmaking> Matchmaking;

public:


	STEAM_CALLBACK(FSteamMatchmakingImpl, OnLobbyDataUpdatedCallback, LobbyDataUpdate_t, CallbackLobbyDataUpdated);

	CCallResult<FSteamMatchmakingImpl, LobbyCreated_t> SteamCallLobbyCreated;

	void OnLobbyCreatedCallback(LobbyCreated_t* pLobbyCreated, bool bIOFailure)
	{
		check(Matchmaking.IsValid());

		if (pLobbyCreated->m_eResult == EResult::k_EResultOK)
		{
			USteamworksManager* Manager = USteamworksManager::Get(Matchmaking.Get());

			ensure(Manager);
			if (Manager)
			{
				Manager->SetLobbyId(pLobbyCreated->m_ulSteamIDLobby);

				ASteamLobby* Lobby = Matchmaking->RestoreLobby();

				Matchmaking->OnLobbyCreated.Broadcast(Lobby);
				Matchmaking->LobbyCreated(Lobby);

			}
		}
	}

	CCallResult<FSteamMatchmakingImpl, LobbyMatchList_t> SteamCallResultLobbyMatchList;

	void OnLobbyMatchListCallback(LobbyMatchList_t* pLobbyMatchList, bool bIOFailure)
	{
		check(Matchmaking.IsValid());

		Matchmaking->LobbyList.Empty();

		for (uint32 i = 0; i < pLobbyMatchList->m_nLobbiesMatching; i++)
		{
			FSteamLobbyInfo Info;

			Info.Id = SteamMatchmaking()->GetLobbyByIndex(i);
			Info.Name = "";
			Info.UpdateData();

			Matchmaking->LobbyList.Add(Info);
		}


		Matchmaking->bRequestingLobbyList = false;
		Matchmaking->OnLobbyListUpdated.Broadcast(Matchmaking.Get());
		Matchmaking->LobbyListUpdated();
	}

	FSteamMatchmakingImpl(ASteamMatchmaking* Owner) :
		Matchmaking(Owner),
		CallbackLobbyDataUpdated(this, &FSteamMatchmakingImpl::OnLobbyDataUpdatedCallback)
	{

	}
};

void FSteamMatchmakingImpl::OnLobbyDataUpdatedCallback(LobbyDataUpdate_t* pCallback)
{
	check(Matchmaking.IsValid());

	if (pCallback->m_bSuccess)
	{

		bool bUpdated = false;

		for (int32 i = 0; i < Matchmaking->LobbyList.Num(); i++)
		{
			FSteamLobbyInfo& Info = Matchmaking->LobbyList[i];
			
			if (Info.Id == pCallback->m_ulSteamIDLobby)
			{
				Info.UpdateData(true);
				bUpdated = true;

				break;
			}
		}

		if (bUpdated)
		{
			Matchmaking->OnLobbyListUpdated.Broadcast(Matchmaking.Get());
			Matchmaking->LobbyListUpdated();
		}
	}
}



ASteamMatchmaking::ASteamMatchmaking()
{

	bRequestingLobbyList = false;
	Impl = nullptr;

	LobbyClass = ASteamLobby::StaticClass();
}

void ASteamMatchmaking::BeginPlay()
{
	ensure(Impl == nullptr);
	Impl = new FSteamMatchmakingImpl(this);


	Super::BeginPlay();
}

void ASteamMatchmaking::BeginDestroy()
{
	Super::BeginDestroy();

	if (Impl)
	{
		delete Impl;
		Impl = nullptr;
	}
}

#define STEAMWORKS_CHECK_MATCHMAKING() if (!SteamMatchmaking()) { UE_LOG(SteamworksLog, Error, TEXT("SteamMatchmaking is null")); ensure(false); return; }

void ASteamMatchmaking::RequestLobbyList()
{
	STEAMWORKS_CHECK_MATCHMAKING();

	if (bRequestingLobbyList) return;

	SteamAPICall_t hSteamAPICall = SteamMatchmaking()->RequestLobbyList();

	check(Impl);
	Impl->SteamCallResultLobbyMatchList.Set(hSteamAPICall, Impl, &FSteamMatchmakingImpl::OnLobbyMatchListCallback);

	bRequestingLobbyList = true;
}

static ELobbyComparison GetSteamSDKEnum(ESteamLobbyComparison ComparisonType)
{
	switch (ComparisonType)
	{
	case ESteamLobbyComparison::EqualToOrLessThan:    return k_ELobbyComparisonEqualToOrLessThan;
	case ESteamLobbyComparison::LessThan:             return k_ELobbyComparisonLessThan;
	case ESteamLobbyComparison::Equal:                return k_ELobbyComparisonEqual;
	case ESteamLobbyComparison::GreaterThan:          return k_ELobbyComparisonGreaterThan;
	case ESteamLobbyComparison::EqualToOrGreaterThan: return k_ELobbyComparisonEqualToOrGreaterThan;
	case ESteamLobbyComparison::NotEqual:             return k_ELobbyComparisonNotEqual;
	};

	ensure(false);
	return k_ELobbyComparisonEqual;
};


static ELobbyDistanceFilter GetSteamSDKEnum(ESteamLobbyDistanceFilter Filter)
{

	switch (Filter)
	{
	case ESteamLobbyDistanceFilter::Close: return k_ELobbyDistanceFilterClose;
	case ESteamLobbyDistanceFilter::Default: return k_ELobbyDistanceFilterDefault;
	case ESteamLobbyDistanceFilter::Far: return k_ELobbyDistanceFilterFar;
	case ESteamLobbyDistanceFilter::Worldwide: return k_ELobbyDistanceFilterWorldwide;
	};

	return k_ELobbyDistanceFilterDefault;
}

void ASteamMatchmaking::AddRequestLobbyListStringFilter(const FString& KeyToMatch, const FString& ValueToMatch, ESteamLobbyComparison ComparisonType)
{
	STEAMWORKS_CHECK_MATCHMAKING();
	SteamMatchmaking()->AddRequestLobbyListStringFilter(TCHAR_TO_ANSI(*KeyToMatch), TCHAR_TO_ANSI(*ValueToMatch), GetSteamSDKEnum(ComparisonType));
}

void ASteamMatchmaking::AddRequestLobbyListNumericalFilter(const FString& KeyToMatch, int32 ValueToMatch, ESteamLobbyComparison ComparisonType)
{
	STEAMWORKS_CHECK_MATCHMAKING();
	SteamMatchmaking()->AddRequestLobbyListNumericalFilter(TCHAR_TO_ANSI(*KeyToMatch), ValueToMatch, GetSteamSDKEnum(ComparisonType));
}

void ASteamMatchmaking::AddRequestLobbyListNearValueFilter(const FString& KeyToMatch, int32 ValueToBeCloseTo)
{
	STEAMWORKS_CHECK_MATCHMAKING();
	SteamMatchmaking()->AddRequestLobbyListNearValueFilter(TCHAR_TO_ANSI(*KeyToMatch), ValueToBeCloseTo);
}

void ASteamMatchmaking::AddRequestLobbyListFilterSlotsAvailable(int32 SlotsAvailable)
{
	STEAMWORKS_CHECK_MATCHMAKING();
	SteamMatchmaking()->AddRequestLobbyListFilterSlotsAvailable(SlotsAvailable);
}

void ASteamMatchmaking::AddRequestLobbyListDistanceFilter(ESteamLobbyDistanceFilter DistanceFilter)
{
	STEAMWORKS_CHECK_MATCHMAKING();
	SteamMatchmaking()->AddRequestLobbyListDistanceFilter(GetSteamSDKEnum(DistanceFilter));
}

void ASteamMatchmaking::CreateLobby()
{
	STEAMWORKS_CHECK_MATCHMAKING();

	SteamAPICall_t hSteamAPICall = SteamMatchmaking()->CreateLobby(ELobbyType::k_ELobbyTypePublic, 8);

	check(Impl);
	Impl->SteamCallLobbyCreated.Set(hSteamAPICall, Impl, &FSteamMatchmakingImpl::OnLobbyCreatedCallback);
}

class ASteamLobby* ASteamMatchmaking::RestoreLobby()
{
	ASteamLobby* Lobby = Cast<ASteamLobby>(UGameplayStatics::BeginSpawningActorFromClass(this, LobbyClass, FTransform::Identity, true, nullptr));

	if (Lobby)
	{
		USteamworksManager* Manager = USteamworksManager::Get(this);

		ensure(Manager);
		if (Manager)
		{
			Lobby->Info.Id = Manager->GetLobbyId();
			Lobby->Info.UpdateData(true);
		}

		UGameplayStatics::FinishSpawningActor(Lobby, FTransform::Identity);
	}

	return Lobby;
}




	

