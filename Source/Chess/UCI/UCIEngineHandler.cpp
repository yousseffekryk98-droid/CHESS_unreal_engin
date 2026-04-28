// Fill out your copyright notice in the Description page of Project Settings.


#include "UCI/UCIEngineHandler.h"
#include "Misc/InteractiveProcess.h"
#include "ChessLogic/CGChessBoard.h"
#include "ChessLogic/CGSquareCoord.h"
#include "ChessLogic/CGTile.h"
#include "ChessLogic/CGPiece.h"
#include "GameLogic/CGChessPlayerController.h"
#include "Internationalization/Regex.h"
#include "GameLogic/CGUtils.h"

UUCIEngineHandler::UUCIEngineHandler()
{

}

bool UUCIEngineHandler::StartEngine(const FString iEnginePath, const int iTurnTime, const int iElo, ACGChessPlayerController* iController, ACGChessBoard* iBoard)
{
	EnginePath = iEnginePath;
	TurnTime = iTurnTime;
	Elo = iElo;
	Controller = iController;
	Board = iBoard;
	if (EngineProc)
	{
		EngineProc->Stop();
		EngineProc.Reset();
	}
	EngineProc = MakeShared<FInteractiveProcess>(iEnginePath, TEXT(""), true);
	EngineProc->OnOutput().BindUFunction(this, TEXT("OnReceive"));
	EngineProc->OnCanceled().BindUFunction(this, TEXT("OnStopped"));
	EngineProc->OnCompleted().BindUFunction(this, TEXT("OnStopped"));
	EngineProc->SendWhenReady(TEXT("uci\n"));
	SetState(EUCIState::STARTING);
	if (!EngineProc->Launch())
	{
		Dbg("Couldn't launch InteractiveProcess!", "");
		SetState(EUCIState::UCI_ERROR);
		return false;
	}
	return true;
}

void UUCIEngineHandler::ApplySettings()
{
	SendCommand(FString::Printf(TEXT("setoption name UCI_LimitStrength value true\nsetoption name UCI_Elo value %d\n"), Elo));
}

void UUCIEngineHandler::StopEngine()
{
	if (EngineProc && EngineProc->IsRunning())
	{
		SendCommand("quit");
	}
}

void UUCIEngineHandler::CheckIfEngineShouldStartThinking()
{
	if (UWorld* w = GetWorld())
	{
		ACGChessPlayerController* pc = w->GetFirstPlayerController<ACGChessPlayerController>();
		if (pc && Board && pc->bIsBlack != Board->IsNextMoveBlack())
		{
			GetNextMove();
		}
	}
}

void UUCIEngineHandler::GetNextMove()
{
	if (Board && Board->IsReadyForNextMove() && IsReady())
	{
		//build the moves
		FString moves(TEXT("moves "));
		bool hasMoves = false;
		for (const FCGUndo& u : Board->Undos)
		{
			if (u.From && u.To)
			{
				u.From->AppendCoordToString(moves, false);
				u.To->AppendCoordToString(moves, false);
				if (u.Promotion)
				{
					moves.Append(u.Promotion->GetFenChar());
				}
				moves.AppendChar(' ');
				hasMoves = true;
			}
		}

		SendCommand(FString::Printf(TEXT("position fen %s %s \ngo\n"), *Board->StartingFen, hasMoves ? *moves : TEXT("")));
		
		GetWorld()->GetTimerManager().SetTimer(TimerHandle, this, &UUCIEngineHandler::OnTurnTimeUp, 1, false, TurnTime);

		SetState(EUCIState::THINKING);
	}
}

void UUCIEngineHandler::BeginDestroy()
{
	if (EngineProc)
	{
		EngineProc->Stop();
	}
	Super::BeginDestroy();
}

bool UUCIEngineHandler::IsReady()
{
	return State == EUCIState::READY && EngineProc && EngineProc->IsRunning();
}

bool UUCIEngineHandler::IsError()
{
	return State == EUCIState::UCI_ERROR;
}

void UUCIEngineHandler::SendCommand(const FString& iCmd)
{
	if (EngineProc)
	{
		EngineProc->SendWhenReady(iCmd);
	}
}

void UUCIEngineHandler::OnTurnTimeUp()
{
	SendCommand("stop\n");
}

void UUCIEngineHandler::OnReceive(const FString& iReceived)
{
	if (iReceived.StartsWith("uciok", ESearchCase::IgnoreCase))
	{
		//uci is ready to receive the settings
		ApplySettings();
		SetState(EUCIState::READY);
	}
	else if (iReceived.StartsWith("info", ESearchCase::IgnoreCase))
	{
		//Dbg("Info: %s", *iReceived+2);
	}
	else if (iReceived.StartsWith("bestmove", ESearchCase::IgnoreCase))
	{
		static const FRegexPattern BestMovePattern(TEXT("bestmove\\s+([^\\s]+)\\s+(?:ponder\\s+([^\\s]+))?"));
		FRegexMatcher matcher(BestMovePattern, iReceived);
		if (matcher.FindNext())
		{
			FString move = matcher.GetCaptureGroup(1).ToLower();
			AsyncTask(ENamedThreads::GameThread, [this, move]() {
				if (Board && move.Len() > 3)
				{
					int x = move[0] - 'a';
					int y = move[1] - '1';
					ACGTile* from = Board->GetTile(FCGSquareCoord(x, y));
					x = move[2] - 'a';
					y = move[3] - '1';
					ACGTile* to = Board->GetTile(FCGSquareCoord(x, y));
					if (from && to)
					{
						if (ACGChessPlayerController* pc = GetWorld()->GetFirstPlayerController<ACGChessPlayerController>())
						{
							pc->ServerMoveToTile(from->OccupiedBy, to);

							if (move.Len() > 4)
							{
								pc->ServerChoosePromotion(move.RightChop(4));
							}
						}
					}
				}
			});
		}
		SetState(EUCIState::READY);
	}
	else
	{
		//Dbg("Received: %s", *iReceived);
	}
}

void UUCIEngineHandler::OnStopped()
{
	SetState(EUCIState::NOT_RUNNING);
}

void UUCIEngineHandler::SetState(EUCIState iNewState)
{
	if (State != iNewState)
	{
		State = iNewState;
		if (!IsInGameThread())
		{
			AsyncTask(ENamedThreads::GameThread, [this]() {
				OnStateChanged.Broadcast(State);
			});
		}
		else
		{
			OnStateChanged.Broadcast(State);
		}
	}
}

