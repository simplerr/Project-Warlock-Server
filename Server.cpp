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

Server::Server()
{
	srand(time(0));

	// Create the RakNet peer
	mRaknetPeer = RakNet::RakPeerInterface::GetInstance();

	// Create the world.
	mWorld = new GLib::World();
	mWorld->Init(GLib::GetGraphics());

	mWorld->AddObjectAddedListener(&Server::OnObjectAdded, this);
	mWorld->AddObjectRemovedListener(&Server::OnObjectRemoved, this);
	mWorld->AddObjectCollisionListener(&Server::OnObjectCollision, this);

	mSkillInterpreter = new ServerSkillInterpreter();
	mCollisionHandler = new CollisionHandler();
	mItemLoader = new ItemLoaderXML("F:\\Users\\Axel\\Documents\\Visual Studio 11\\Projects\\Project Warlock\\Project Warlock\\items.xml");	// [NOTE]!
	mMessageHandler = new ServerMessageHandler(this);

	// Connect the graphics light list.
	GLib::GetGraphics()->SetLightList(mWorld->GetLights());

	mTickRate = 1.0f / 100.0f;	// 20 ticks per second.

	// Add a test player.
	mTestDoll = new Player();
	mTestDoll->SetPosition(XMFLOAT3(0, 0, 10));
	mTestDoll->SetScale(XMFLOAT3(0.1f, 0.1f, 0.1f));	// [NOTE]
	mWorld->AddObject(mTestDoll);

	mTickCounter = mDamageCounter = 0.0f;

	mRoundHandler = new RoundHandler();
	mRoundHandler->SetServer(this);
	mRoundHandler->SetPlayerList(&mPlayerList);
}

Server::~Server()
{
	delete mWorld;
	delete mSkillInterpreter;
	delete mMessageHandler;
	delete mCollisionHandler;
	delete mItemLoader;
	delete mRoundHandler;

	mRaknetPeer->Shutdown(300);
	RakNet::RakPeerInterface::DestroyInstance(mRaknetPeer);
}

void Server::Update(GLib::Input* pInput, float dt)
{
	// Update the world.
	mWorld->Update(dt);

	// Update the world handler.
	mRoundHandler->Update(pInput, dt);

	// Deal damage to players outside the arena.
	mDamageCounter += dt;
	if(mDamageCounter > 0.1f)
	{
		for(int i = 0; i < mPlayerList.size(); i++) {
			if(mPlayerList[i]->GetEliminated())
				continue;

			XMFLOAT3 pos = mPlayerList[i]->GetPosition();
			float distFromCenter = sqrt(pos.x * pos.x + pos.z * pos.z);

			// Arena is 60 units in radius.
			if(distFromCenter > 60.0f) {
				mPlayerList[i]->SetHealth(mPlayerList[i]->GetHealth() - mCvars.GetCvarValue(Cvars::LAVA_DMG));

				// Died?
				if(mPlayerList[i]->GetHealth() <= 0) 
					PlayerEliminated(mPlayerList[i], mPlayerList[i]->GetLastHitter());
			}
		}
		mDamageCounter = 0.0f;
	}

	// Broadcast the world at a fixed rate.
	mTickCounter += dt;
	if(mTickCounter > mTickRate) {
		// Find out if there is only 1 player alive (round ended).
		string winner;
		if(mRoundHandler->HasRoundEnded(winner))
		{
			// Add gold to the winner.
			Player* winningPlayer = (Player*)mWorld->GetObjectByName(winner);
			winningPlayer->SetGold(winningPlayer->GetGold() + GetCvarValue(Cvars::GOLD_PER_WIN));

			// Send round ended message.
			RakNet::BitStream bitstream;
			bitstream.Write((unsigned char)NMSG_ROUND_ENDED);
			bitstream.Write(winner.c_str());
			SendClientMessage(bitstream);

			// Increment winners score.
			mScoreMap[winner]++;
		}

		BroadcastWorld();
		mRoundHandler->BroadcastStateTimer();
		mTickCounter = 0.0f;
	}

	// Listen for incoming packets.
	ListenForPackets();
}

void Server::Draw(GLib::Graphics* pGraphics)
{
	mWorld->Draw(pGraphics);

	mRoundHandler->Draw(pGraphics);

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

void Server::PlayerEliminated(Player* pKilled, Player* pEliminator)
{
	// Add gold to the killer. 
	if(pEliminator != nullptr)
		pEliminator->SetGold(pEliminator->GetGold() + GetCvarValue(Cvars::GOLD_PER_KILL));

	// Tell the clients about the kill.
	RakNet::BitStream bitstream;
	bitstream.Write((unsigned char)NMSG_PLAYER_ELIMINATED);
	bitstream.Write(pKilled->GetName().c_str());
	bitstream.Write(pEliminator == nullptr ? "himself" : pEliminator->GetName().c_str());
	SendClientMessage(bitstream);
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

//! Gets called in World::AddObject().
void Server::OnObjectAdded(GLib::Object3D* pObject)
{
	// Add player to mPlayerList.
	if(pObject->GetType() == GLib::PLAYER)
		mPlayerList.push_back((Player*)pObject);
}

//! Gets called in World::RemoveObject().
void Server::OnObjectRemoved(GLib::Object3D* pObject)
{
	// Remove player from mPlayerList:
	if(pObject->GetType() == GLib::PLAYER) 
		RemovePlayer(pObject->GetId());

	RakNet::BitStream bitstream;
	bitstream.Write((unsigned char)NMSG_OBJECT_REMOVED);
	bitstream.Write(pObject->GetId());
	mRaknetPeer->Send(&bitstream, HIGH_PRIORITY, RELIABLE, 0, RakNet::UNASSIGNED_SYSTEM_ADDRESS, true);
}

void Server::OnObjectCollision(GLib::Object3D* pObjectA, GLib::Object3D* pObjectB)
{
	GLib::ObjectType typeA = pObjectA->GetType();
	GLib::ObjectType typeB = pObjectB->GetType();

	Projectile* projectile = pObjectA->GetType() == GLib::PROJECTILE ? (Projectile*)pObjectA : (Projectile*)pObjectB;
	Player* player = pObjectA->GetType() == GLib::PLAYER ? (Player*)pObjectA : (Player*)pObjectB;

	// Player - Projectile collision only.
	if((pObjectB->GetType() == GLib::PLAYER && pObjectA->GetType() == GLib::PROJECTILE) || (pObjectB->GetType() == GLib::PROJECTILE && pObjectA->GetType() == GLib::PLAYER))
	{
		if(projectile->GetOwner() == player->GetId())
			return;
	
		if(mRoundHandler->GetArenaState().state != SHOPPING_STATE)
		{
			// Checks what skill the projectile is and uses the XML data to determine the skill attributes
			// depending on the skill level.
			mCollisionHandler->HandleCollision(player, projectile, this, mItemLoader, mCvars.GetCvarValue(Cvars::PROJECTILE_IMPULSE) / 10.0f);

			// Let the clients now about the changes immediately.
			BroadcastWorld();

			// Tell all clients about the collision.
			RakNet::BitStream bitstream;
			bitstream.Write((unsigned char)NMSG_PROJECTILE_PLAYER_COLLISION);
			bitstream.Write(projectile->GetId());	// Projectile Id.
			bitstream.Write(player->GetId());	// Player Id.
			SendClientMessage(bitstream);
		}

		// Remove the projectile.
		mWorld->RemoveObject(projectile);
	}
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
			mMessageHandler->HandleNewConnection(bitstream, pPacket->systemAddress);
			break;
		case ID_CONNECTION_LOST:
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

void Server::BroadcastWorld()
{
	// Broadcast world data at a set tickrate.
	GLib::ObjectList* objects = mWorld->GetObjects();
	for(int i = 0; i < objects->size(); i++)
	{
		GLib::Object3D* object = (GLib::Object3D*)objects->operator[](i);
		XMFLOAT3 pos = object->GetPosition();

		RakNet::BitStream bitstream;
		bitstream.Write((unsigned char)NMSG_WORLD_UPDATE);
		bitstream.Write(object->GetType());
		bitstream.Write((unsigned char)object->GetId());
		bitstream.Write(pos.x);
		bitstream.Write(pos.y);
		bitstream.Write(pos.z);

		if(object->GetType() == GLib::PLAYER)
		{
			Player* player = (Player*)object;
			bitstream.Write(player->GetHealth());
			bitstream.Write(player->GetGold());
		}

		mRaknetPeer->Send(&bitstream, HIGH_PRIORITY, RELIABLE, 0, RakNet::UNASSIGNED_SYSTEM_ADDRESS, true);
	}
}

//! Removes a player from mPlayerList.
void Server::RemovePlayer(int id)
{
	// Loop through all objects and find out which one to delete.
	for(auto iter =  mPlayerList.begin(); iter != mPlayerList.end(); iter++)
	{
		if((*iter)->GetId() == id) {
			mPlayerList.erase(iter);
			break;
		}
		else	
			iter++;
	}
}

string Server::RemovePlayer(RakNet::SystemAddress adress)
{
	string name = "#NOVALUE";

	GLib::ObjectList* objects = mWorld->GetObjects();
	for(int i = 0; i < objects->size(); i++)
	{
		GLib::Object3D* object = objects->operator[](i);
		if(object->GetType() == GLib::PLAYER)
		{
			Player* player = (Player*)object;
			if(player->GetSystemAdress() == adress) {
				name = player->GetName();
				mWorld->RemoveObject(player->GetId());
			}
		}
	}

	return name;
}

vector<string> Server::GetConnectedClients()
{
	vector<string> clients;
	GLib::ObjectList* objects = mWorld->GetObjects();
	for(int i = 0; i < objects->size(); i++)
	{
		GLib::Object3D* object = objects->operator[](i);
		if(object->GetType() == GLib::PLAYER)
			clients.push_back(object->GetName());
	}

	return clients;
}

RakNet::RakPeerInterface* Server::GetRaknetPeer()
{
	return mRaknetPeer;
}

GLib::World* Server::GetWorld()
{
	return mWorld;
}

RoundHandler* Server::GetRoundHandler()
{
	return mRoundHandler;
}

bool Server::IsHost(string name)
{
	return true;
}

bool Server::IsCvarCommand(string cmd)
{
	// [NOTE] RESTART_ROUND!!!
	return (cmd == Cvars::GIVE_GOLD || cmd == Cvars::RESTART_ROUND || cmd == Cvars::START_GOLD || cmd == Cvars::SHOP_TIME || cmd == Cvars::ROUND_TIME || cmd == Cvars::NUM_ROUNDS ||
			cmd == Cvars::GOLD_PER_KILL || cmd == Cvars::GOLD_PER_WIN || cmd == Cvars::LAVA_DMG || cmd == Cvars::PROJECTILE_IMPULSE);
}

int	Server::GetCvarValue(string cvar)
{
	return mCvars.GetCvarValue(cvar);
}

void Server::SetScore(string name, int score)
{
	mScoreMap[name] = score;
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