#include <time.h>
#include "ServerMessageHandler.h"
#include "CollisionHandler.h"
#include "Server.h"
#include "Input.h"
#include "BitStream.h"
#include "World.h"
#include "Object3D.h"
#include "Actor.h"
#include "MessageIdentifiers.h"
#include "Projectile.h"
#include "Graphics.h"
#include "NetworkMessages.h"
#include "Player.h"
#include "ServerSkillInterpreter.h"
#include "ItemLoaderXML.h"
#include "RoundHandler.h"
#include "ServerArena.h"
#include "Database.h"
#include "Config.h"
#include "ServerCvars.h"
#include "Console.h"

Server::Server()
{
	srand(time(0));

	// Create the RakNet peer
	mRaknetPeer = RakNet::RakPeerInterface::GetInstance();

	mSkillInterpreter = new ServerSkillInterpreter();
	mItemLoader = new ItemLoaderXML("data/items.xml");	// [NOTE]!
	mMessageHandler = new ServerMessageHandler(this);

	mArena = new ServerArena(this);

	mRoundHandler = new RoundHandler();
	mRoundHandler->SetServer(this);
	mRoundHandler->SetPlayerList(mArena->GetPlayerListPointer());

	// Load the server and nickname from config.txt.
	Config config("data/config.txt");
	mServerName =  config.serverName;
	mHostName = config.nickName;

	mDatabase = new Database();
	mDatabase->AddServer(mHostName, mServerName, mDatabase->GetPublicIp(), mDatabase->GetLocalIp());

	// Temp
	//mRoundHandler->StartLobbyCountdown();

	mCvars.LoadFromFile("data/cvars.cfg");

	mInLobby = true;
	
	gConsole->AddLine("Server successfully started!");
	gConsole->AddLine(mServerName.c_str());
}

Server::~Server()
{
	// Send NMSG_SERVER_SHUTDOWN to all connected players.
	RakNet::BitStream bitstream;
	bitstream.Write((unsigned char)NMSG_SERVER_SHUTDOWN);
	SendClientMessage(bitstream);

	delete mSkillInterpreter;
	delete mMessageHandler;
	delete mItemLoader;
	delete mRoundHandler;
	delete mArena;

	mDatabase->RemoveServer(mHostName);
	delete mDatabase;

	mRaknetPeer->Shutdown(300);
	RakNet::RakPeerInterface::DestroyInstance(mRaknetPeer);
}

void Server::Update(GLib::Input* pInput, float dt)
{
	// Update the world handler.
	mRoundHandler->Update(pInput, dt);
	mArena->Update(pInput, dt);

	// Listen for incoming packets.
	ListenForPackets();
}

void Server::Draw(GLib::Graphics* pGraphics)
{
	mRoundHandler->Draw(pGraphics);
	mArena->Draw(pGraphics);

	DrawScores(pGraphics);
}

void Server::SendClientMessage(RakNet::BitStream& bitstream, bool broadcast, RakNet::SystemAddress adress)
{
	// Send it to all other clients.
	mRaknetPeer->Send(&bitstream, HIGH_PRIORITY, RELIABLE_ORDERED, 0, adress, broadcast);
}

void Server::StartGame()
{
	mRoundHandler->StartRound();
	mArena->StartGame();
	mInLobby = false;

	RakNet::BitStream bitstream;
	bitstream.Write((unsigned char)NMSG_GAME_STARTED);
	SendClientMessage(bitstream);

	gConsole->AddLine("Game starting!");
}

void Server::DrawScores(GLib::Graphics* pGraphics)
{
	string scoreList = "Scores:\n";
	for(auto iter = mScoreMap.begin(); iter != mScoreMap.end(); iter++)
	{
		char score[10];
		sprintf(score, "%i", (*iter).second);
		scoreList += (*iter).first + ": " + score + "\n";
	}

	pGraphics->DrawText(scoreList, 10, 100, 20);
}

bool Server::StartServer()
{
	if(mRaknetPeer->Startup(10, &RakNet::SocketDescriptor(27020, 0), 1) == RakNet::RAKNET_STARTED)	{
		mRaknetPeer->SetMaximumIncomingConnections(10);
		return true;
	}
	else
		return false;
}

bool Server::ListenForPackets()
{
	RakNet::Packet *packet = nullptr;
	packet = mRaknetPeer->Receive();

	if(packet != nullptr)	{
		HandlePacket(packet);
		mRaknetPeer->DeallocatePacket(packet);
	}

	return true;
}

bool Server::HandlePacket(RakNet::Packet* pPacket)
{
	// Receive the packet.
	RakNet::BitStream bitstream((unsigned char*)pPacket->data, pPacket->length, false);
	unsigned char packetID;

	// Read the packet id.
	bitstream.Read(packetID);

	// Switch the packet id.
	switch(packetID)
	{
		case ID_NEW_INCOMING_CONNECTION:
			mDatabase->IncrementPlayerCounter(mServerName, 1);
			mMessageHandler->HandleNewConnection(bitstream, pPacket->systemAddress);
			break;
		case ID_CONNECTION_LOST:
			mDatabase->IncrementPlayerCounter(mServerName, -1);
			mMessageHandler->HandleConnectionLost(bitstream, pPacket->systemAddress);
			break;
		case NMSG_CLIENT_CONNECTION_DATA:
			mMessageHandler->HandleConnectionData(bitstream, pPacket->systemAddress);
			break;
		case NMSG_REQUEST_CLIENT_NAMES:
			mMessageHandler->HandleNamesRequest(bitstream, pPacket->systemAddress);
			break;
		case NMSG_TARGET_ADDED:
			mMessageHandler->HandleTargetAdded(bitstream);
			break;
		case NMSG_SKILL_CAST:
			mMessageHandler->HandleSkillCasted(bitstream);
			break;
		case NMSG_ITEM_ADDED:
			mMessageHandler->HandleItemAdded(bitstream, pPacket->systemAddress);
			break;
		case NMSG_ITEM_REMOVED:
			mMessageHandler->HandleItemRemoved(bitstream, pPacket->systemAddress);
			break;
		case NMSG_GOLD_CHANGE:
			mMessageHandler->HandleGoldChange(bitstream, pPacket->systemAddress);
			break;
		case NMSG_CHAT_MESSAGE_SENT:
			mMessageHandler->HandleChatMessage(bitstream, pPacket->systemAddress);
			break;
		case NMSG_REQUEST_CVAR_LIST:
			mMessageHandler->HandleCvarListRequest(bitstream, pPacket->systemAddress);
			break;
		case NMSG_START_COUNTDOWN:
			mRoundHandler->StartLobbyCountdown();
			break;
		case NMSG_REQUEST_REMATCH:
			mMessageHandler->HandleRematchRequest(bitstream);
			break;
	}

	return true;
}

void Server::AddClientChatText(string text, COLORREF color, bool broadcast, RakNet::SystemAddress adress)
{
	// Send cvar change message.
	RakNet::BitStream bitstream;
	bitstream.Write((unsigned char)NMSG_ADD_CHAT_TEXT);
	bitstream.Write(text.c_str());
	bitstream.Write(color);
	SendClientMessage(bitstream, broadcast, adress);
}

vector<string> Server::GetConnectedClients()
{
	vector<string> clients;
	vector<Player*>* players = mArena->GetPlayerListPointer();

	for(int i = 0; i < players->size(); i++)
		clients.push_back(players->operator[](i)->GetName());

	return clients;
}

bool Server::IsCvarCommand(string cmd)
{
	// [NOTE] RESTART_ROUND!!!
	for(auto iter = mCvars.CvarMap.begin(); iter != mCvars.CvarMap.end(); iter++) 
	{
		if((*iter).first == cmd)
			return true;
	}

	return (cmd == Cvars::GIVE_GOLD || cmd == Cvars::RESTART_ROUND);
}

string Server::RemovePlayer(RakNet::SystemAddress adress)
{
	return mArena->RemovePlayer(adress);
}

RakNet::RakPeerInterface* Server::GetRaknetPeer()
{
	return mRaknetPeer;
}

GLib::World* Server::GetWorld()
{
	return mArena->GetWorld();
}

RoundHandler* Server::GetRoundHandler()
{
	return mRoundHandler;
}

bool Server::IsHost(string name)
{
	return true;
}

float Server::GetCvarValue(string cvar)
{
	return mCvars.GetCvarValue(cvar);
}

void Server::SetScore(string name, int score)
{
	mScoreMap[name] = score;
}

void Server::AddScore(string name, int score)
{
	if(mScoreMap.find(name) != mScoreMap.end())
		mScoreMap[name]+= score;
}

ServerSkillInterpreter* Server::GetSkillInterpreter()
{
	return mSkillInterpreter;
}

ItemLoaderXML* Server::GetItemLoader()
{
	return mItemLoader;
}

void Server::SetCvarValue(string cvar, float value)
{
	mCvars.SetCvarValue(cvar, value);
}

CurrentState Server::GetArenaState()
{
	return mRoundHandler->GetArenaState().state;
}

bool Server::IsRoundOver(string& winner)
{
	return mRoundHandler->HasRoundEnded(winner);
}

bool Server::IsGameOver()
{
	if(mRoundHandler->GetCompletedRounds() >= mCvars.GetCvarValue(Cvars::NUM_ROUNDS))
		return true;
	else
		return false;
}

void Server::AddRoundCompleted()
{
	mRoundHandler->AddRoundCompleted();
}

ServerCvars	Server::GetCvars()
{
	return mCvars;
}

ServerArena* Server::GetArena()
{
	return mArena;
}

string Server::GetHostName()
{
	return mHostName;
}

void Server::ResetScores()
{
	for(auto iter = mScoreMap.begin(); iter != mScoreMap.end(); iter++)
		(*iter).second = 0;
}

void Server::StripItems()
{

}

bool Server::IsInLobby()
{
	return mInLobby;
}