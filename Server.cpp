#include <time.h>
#include "Utils.h"
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

Server::Server()
{
	srand(time(0));

	// Create the RakNet peer
	mRaknetPeer = RakNet::RakPeerInterface::GetInstance();

	mArena = new ServerArena(this);

	mSkillInterpreter = new ServerSkillInterpreter();
	mItemLoader = new ItemLoaderXML("F:\\Users\\Axel\\Documents\\Visual Studio 11\\Projects\\Project Warlock\\Project Warlock\\items.xml");	// [NOTE]!
	mMessageHandler = new ServerMessageHandler(this);

	mRoundHandler = new RoundHandler();
	mRoundHandler->SetServer(this);
	mRoundHandler->SetPlayerList(mArena->GetPlayerListPointer());

	mServerName = "Fun server [24/7]";
	mDatabase = new Database();
	mDatabase->AddServer(mServerName, mDatabase->GetPublicIp(), mDatabase->GetLocalIp());
}

Server::~Server()
{
	delete mSkillInterpreter;
	delete mMessageHandler;
	delete mItemLoader;
	delete mRoundHandler;
	delete mArena;

	mDatabase->RemoveServer("host");
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

	char buffer[244];
	sprintf(buffer, "Timer: %.2f", mRoundHandler->GetArenaState().elapsed);
	pGraphics->DrawText(buffer, 10, 40, 14);

	DrawScores(pGraphics);
}

void Server::SendClientMessage(RakNet::BitStream& bitstream, bool broadcast, RakNet::SystemAddress adress)
{
	// Send it to all other clients.
	mRaknetPeer->Send(&bitstream, HIGH_PRIORITY, RELIABLE_ORDERED, 0, adress, broadcast);
}

void Server::DrawScores(GLib::Graphics* pGraphics)
{
	string scoreList;
	for(auto iter = mScoreMap.begin(); iter != mScoreMap.end(); iter++)
	{
		char score[10];
		sprintf(score, "%i", (*iter).second);
		scoreList += (*iter).first + ": " + score + "\n";
	}

	pGraphics->DrawText(scoreList, 10, 100, 14);
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
			mMessageHandler->HandleChatMessage(bitstream);
			break;
		case NMSG_REQUEST_CVAR_LIST:
			mMessageHandler->HandleCvarListRequest(bitstream, pPacket->systemAddress);
			break;
	}

	return true;
}

void Server::AddClientChatText(string text, COLORREF color)
{
	// Send cvar change message.
	RakNet::BitStream bitstream;
	bitstream.Write((unsigned char)NMSG_ADD_CHAT_TEXT);
	bitstream.Write(text.c_str());
	bitstream.Write(color);
	SendClientMessage(bitstream);
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
	return (cmd == Cvars::GIVE_GOLD || cmd == Cvars::RESTART_ROUND || cmd == Cvars::START_GOLD || cmd == Cvars::SHOP_TIME || cmd == Cvars::ROUND_TIME || cmd == Cvars::NUM_ROUNDS ||
		cmd == Cvars::GOLD_PER_KILL || cmd == Cvars::GOLD_PER_WIN || cmd == Cvars::LAVA_DMG || cmd == Cvars::PROJECTILE_IMPULSE);
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

int	Server::GetCvarValue(string cvar)
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

void Server::SetCvarValue(string cvar, int value)
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