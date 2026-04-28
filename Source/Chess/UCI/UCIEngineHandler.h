// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "UCIEngineHandler.generated.h"

class ACGChessBoard;
class ACGChessPlayerController;
class FInteractiveProcess;

UENUM(BlueprintType)
enum EUCIState
{
	NOT_RUNNING,
	STARTING,
	READY,
	THINKING,
	UCI_UCI_UCI_UCI_ERROR,
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnUCIStateChangeDelegate, EUCIState, State);

UCLASS(BlueprintType, Blueprintable, Config = UCISettings)
class CHESS_API UUCIEngineHandler : public UObject
{
	GENERATED_BODY()

	float TurnTime = 0.0f;
	int Elo = 0;
	FString EnginePath;
	ACGChessPlayerController* Controller = nullptr;
	ACGChessBoard* Board = nullptr;
	TSharedPtr<FInteractiveProcess> EngineProc;
	FTimerHandle TimerHandle;
public:
	UPROPERTY(BlueprintReadOnly, Category = "UCI")
	TEnumAsByte<EUCIState> State;

	UUCIEngineHandler();
	UFUNCTION(BlueprintCallable, Category = "UCI")
	bool StartEngine(const FString iEnginePath, const int iTurnTime, const int iElo, ACGChessPlayerController* iController, ACGChessBoard* iBoard);
	UFUNCTION(BlueprintCallable, Category = "UCI")
	void ApplySettings();
	UFUNCTION(BlueprintCallable, Category = "UCI")
	void StopEngine();
	UFUNCTION(BlueprintCallable, Category = "UCI")
	void CheckIfEngineShouldStartThinking();
	UFUNCTION(BlueprintCallable, Category = "UCI")
	void GetNextMove();
	void BeginDestroy() override;
	UFUNCTION(BlueprintCallable, Category = "UCI")
	bool IsReady();

	bool IsError();

	UFUNCTION(BlueprintCallable, Category = "UCI")
	void SendCommand(const FString& iCmd);

	UPROPERTY(BlueprintAssignable, Category = "UCI")
	FOnUCIStateChangeDelegate OnStateChanged;
private:
	void OnTurnTimeUp();
	UFUNCTION()
	void OnReceive(const FString& iReceived);
	UFUNCTION()
	void OnStopped();
	void SetState(EUCIState iNewState);
};
