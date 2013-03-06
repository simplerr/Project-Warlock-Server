#include "RoundHandler.h"
#include "NetworkMessages.h"
#include "BitStream.h"
#include "Server.h"
#include "Input.h"
#include "Graphics.h"
#include "Player.h"

RoundHandler::RoundHandler()
{
	mEndCounter = 0.0f;
	mCompletedRounds = 0;
	mRoundEnded = false;
	mLobbyCountdownActive = false;
	mGameOver = false;
	SetServer(nullptr);
	InitShoppingState(mArenaState, true);
}

RoundHandler::~RoundHandler()
{

}

void RoundHandler::Update(GLib::Input* pInput, float dt)
{
	// Reset completed rounds with 'R' (note).
	if(pInput->KeyPressed('R'))
		mCompletedRounds = 0;

	if(mServer->IsGameOver()) {
		mGameOver = true;
		return;
	}

	mArenaState.elapsed += dt;

	// Start new round after 5 seconds.
	if(mRoundEnded && mArenaState.elapsed > 5.0f) {
		mCompletedRounds++;
		StartRound();
		mRoundEnded = false;
	}

	// Lobby countdown [HACK][TODO][NOTE].
	static float lobbyCountDownDelta = 0;
	if(mLobbyCountdownActive)
	{
		lobbyCountDownDelta += dt;
		mLobbyCountdown -= dt;

		if(mLobbyCountdown > 0 && lobbyCountDownDelta > 1) {
			char buffer[64];
			sprintf(buffer, "%i", (int)mLobbyCountdown);
			mServer->AddClientChatText(string(buffer) + "\n", GLib::ColorRGBA(0, 255, 0, 255));
			lobbyCountDownDelta = 0.0f;
		}

		// Start the round.
		if(mLobbyCountdown <= 0) {
			mLobbyCountdownActive = false;
			mServer->StartGame();
		}
	}

	// TESTING [NOTE]! !!!
	if(mArenaState.state == SHOPPING_STATE && pInput->KeyPressed(VK_SPACE))
	{
		InitPlayingState(mArenaState, true);

		RakNet::BitStream bitstream;
		bitstream.Write((unsigned char)NMSG_CHANGETO_PLAYING);
		mServer->SendClientMessage(bitstream);
	}
	else if(mArenaState.state == PLAYING_STATE && pInput->KeyPressed(VK_SPACE))
	{
		InitShoppingState(mArenaState, true);

		RakNet::BitStream bitstream;
		bitstream.Write((unsigned char)NMSG_CHANGETO_SHOPPING);
		mServer->SendClientMessage(bitstream);
	}
}

void RoundHandler::Draw(GLib::Graphics* pGraphics)
{
	if(mArenaState.state == SHOPPING_STATE)
		pGraphics->DrawText("Shopping State", 10, 10, 20);
	else if(mArenaState.state == PLAYING_STATE)
		pGraphics->DrawText("Playing State", 10, 10, 20);
}

void RoundHandler::StartRound()
{
	for(int i = 0; i < mPlayerList->size(); i++)
	{
		mPlayerList->operator[](i)->SetPosition(XMFLOAT3(rand() % 4, 2, rand() % 4));
		mPlayerList->operator[](i)->SetEliminated(false);
		mPlayerList->operator[](i)->Init();
		mPlayerList->operator[](i)->RemoveStatusEffects();
		mPlayerList->operator[](i)->ClearTargetQueue();
	}

	InitShoppingState(mArenaState, true);

	RakNet::BitStream bitstream;
	bitstream.Write((unsigned char)NMSG_ROUND_START);
	mServer->SendClientMessage(bitstream);
	mRoundEnded = false;
}
bool RoundHandler::HasRoundEnded(string& winner)
{
	int numAlive = 0;
	for(int i = 0; i < mPlayerList->size(); i++)
	{
		if(!mPlayerList->operator[](i)->GetEliminated()) {
			winner = mPlayerList->operator[](i)->GetName();
			numAlive++;
		}
	}

	bool ended = false;
	if((numAlive <= 1) && mPlayerList->size() > 1 && !mRoundEnded) {
		ended = true;
		mRoundEnded = true;
		mArenaState.elapsed = 0.0f;
	}


	return ended;
}

void RoundHandler::BroadcastStateTimer()
{
	// Change to playing state if shopping time expired.
	if(mArenaState.state == SHOPPING_STATE && mArenaState.elapsed >= mServer->GetCvarValue(Cvars::SHOP_TIME))
	{
		InitPlayingState(mArenaState, true);

		RakNet::BitStream bitstream;
		bitstream.Write((unsigned char)NMSG_CHANGETO_PLAYING);
		mServer->SendClientMessage(bitstream);
	}

	RakNet::BitStream bitstream;
	bitstream.Write((unsigned char)NMSG_STATE_TIMER);
	bitstream.Write(mArenaState.elapsed);
	mServer->SendClientMessage(bitstream);
}

void RoundHandler::SetPlayerList(vector<Player*>* pPlayerList)
{
	mPlayerList = pPlayerList;
}

void RoundHandler::SetServer(Server* pServer)
{
	mServer = pServer;
}

ArenaState RoundHandler::GetArenaState()
{
	return mArenaState;
}

void RoundHandler::StartLobbyCountdown()
{
	mLobbyCountdownActive = true;
	mServer->AddClientChatText("Game starting in\n", GLib::ColorRGBA(0, 255, 0, 255));
	mLobbyCountdown = 5.0f;
}

int RoundHandler::GetCompletedRounds()
{
	return mCompletedRounds;
}

void RoundHandler::AddRoundCompleted()
{
	mCompletedRounds++;
}