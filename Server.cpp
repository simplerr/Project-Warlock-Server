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

Server::Server()
{
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

	// Connect the graphics light list.
	GLib::GetGraphics()->SetLightList(mWorld->GetLights());

	mTickRate = 1.0f / 100.0f;	// 20 ticks per second.

	// Add a test player.
	mTestDoll = new Player();
	mTestDoll->SetPosition(XMFLOAT3(0, 0, 10));
	mTestDoll->SetScale(XMFLOAT3(0.1f, 0.1f, 0.1f));	// [NOTE]
	mWorld->AddObject(mTestDoll);

	mTickCounter = mDamageCounter = 0.0f;
}

Server::~Server()
{
	delete mWorld;
	delete mSkillInterpreter;
	delete mCollisionHandler;

	// Tell the opponent that you left
	/*RakNet::BitStream bitstream;
	bitstream.Write((unsigned char)ID_LEFT_GAME);
	mRaknetPeer->Send(&bitstream, HIGH_PRIORITY, RELIABLE_ORDERED, 0, RakNet::UNASSIGNED_SYSTEM_ADDRESS, true);*/

	mRaknetPeer->Shutdown(300);
	RakNet::RakPeerInterface::DestroyInstance(mRaknetPeer);
}

void Server::Update(GLib::Input* pInput, float dt)
{
	// Update the world.
	mWorld->Update(dt);
	
	// Deal damage to players outside the arena.
	mDamageCounter += dt;
	if(mDamageCounter > 0.1f)
	{
		for(int i = 0; i < mPlayerList.size(); i++) {
			XMFLOAT3 pos = mPlayerList[i]->GetPosition();
			float distFromCenter = sqrt(pos.x * pos.x + pos.z * pos.z);

			// Arena is 60 units in radius.
			if(distFromCenter > 60.0f)
				mPlayerList[i]->SetHealth(mPlayerList[i]->GetHealth() - 1.0f);

		}
		mDamageCounter = 0.0f;
	}

	// Broadcast the world at a fixed rate.
	mTickCounter += dt;
	if(mTickCounter > mTickRate) {
		BroadcastWorld();
		mTickCounter = 0.0f;
	}

	// Listen for incoming packets.
	ListenForPackets();
}

void Server::Draw(GLib::Graphics* pGraphics)
{
	mWorld->Draw(pGraphics);
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
	
		// Checks what skill the projectile is and uses the XML data to determine the skill attributes
		// depending on the skill level.
		mCollisionHandler->HandleCollision(player, projectile);

		// Let the clients now about the changes immediately.
		BroadcastWorld();

		// Tell all clients about the collision.
		RakNet::BitStream bitstream;
		bitstream.Write((unsigned char)NMSG_PROJECTILE_PLAYER_COLLISION);
		bitstream.Write(projectile->GetId());	// Projectile Id.
		bitstream.Write(player->GetId());	// Player Id.
		SendClientMessage(bitstream);

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
			HandleNewConnection(bitstream, pPacket->systemAddress);
			break;
		case ID_CONNECTION_LOST:
			HandleConnectionLost(bitstream, pPacket->systemAddress);
			break;
		case NMSG_CLIENT_CONNECTION_DATA:
			HandleConnectionData(bitstream, pPacket->systemAddress);
			break;
		case NMSG_REQUEST_CLIENT_NAMES:
			HandleNamesRequest(bitstream, pPacket->systemAddress);
			break;
		case NMSG_TARGET_ADDED:
			HandleTargetAdded(bitstream);
			break;
		case NMSG_SKILL_CAST:
			HandleSkillCasted(bitstream);
			break;
	}

	return true;
}

void Server::HandleNewConnection(RakNet::BitStream& bitstream, RakNet::SystemAddress adress)
{
	// Send connection successful message back.
	// The message contains the players already connected.
	RakNet::BitStream sendBitstream;
	sendBitstream.Write((unsigned char)NMSG_CONNECTION_SUCCESS);
	sendBitstream.Write(mWorld->GetNumObjects(GLib::PLAYER));

	GLib::ObjectList* objects = mWorld->GetObjects();
	for(int i = 0; i < objects->size(); i++)
	{
		GLib::Object3D* object = objects->operator[](i);
		if(object->GetType() == GLib::PLAYER) {
			sendBitstream.Write(object->GetName().c_str());
			sendBitstream.Write(object->GetId());
			sendBitstream.Write(object->GetPosition());
		}
	}

	mRaknetPeer->Send(&sendBitstream, HIGH_PRIORITY, RELIABLE, 0, adress, false);
}

void Server::HandleConnectionLost(RakNet::BitStream& bitstream, RakNet::SystemAddress adress)
{
	RakNet::BitStream sendBitstream;

	string name = RemovePlayer(adress);
	sendBitstream.Write((unsigned char)NMSG_PLAYER_DISCONNECTED);
	sendBitstream.Write(name.c_str());	

	OutputDebugString(string(name + " has disconnected!").c_str());

	// Tell the other clients about the disconnect.
	mRaknetPeer->Send(&sendBitstream, HIGH_PRIORITY, RELIABLE_ORDERED, 0, RakNet::UNASSIGNED_SYSTEM_ADDRESS, true);
}

void Server::HandleTargetAdded(RakNet::BitStream& bitstream)
{
	char name[244];
	unsigned char id;
	float x, y, z;
	bool clear;
	bitstream.Read(name);
	bitstream.Read(id);
	bitstream.Read(x);
	bitstream.Read(y);
	bitstream.Read(z);
	bitstream.Read(clear);

	GLib::ObjectList* objects = mWorld->GetObjects();
	for(int i = 0; i < objects->size(); i++)
	{
		Actor* actor = (Actor*)objects->operator[](i);
		if(actor->GetId() == id && actor->GetSpeed() < 0.09f ) {
			actor->AddTarget(XMFLOAT3(x, y, z), clear);

			// Send the TARGET_ADDED to all clients.
			mRaknetPeer->Send(&bitstream, IMMEDIATE_PRIORITY, UNRELIABLE, 0, RakNet::UNASSIGNED_SYSTEM_ADDRESS, true);
			break;
		}
	}
}

void Server::HandleConnectionData(RakNet::BitStream& bitstream, RakNet::SystemAddress adress)
{
	char buffer[244];
	bitstream.Read(buffer);
	string name = buffer;

	// Add a new player to the World.
	Player* player = new Player();
	player->SetName(name);
	player->SetScale(XMFLOAT3(0.1f, 0.1f, 0.1f));	// [NOTE]
	player->SetSystemAdress(adress);
	player->SetVelocity(XMFLOAT3(0, 0, -0.3f));
	mWorld->AddObject(player);

	OutputDebugString(string(name + " has connected!\n").c_str());

	// [TODO] Add model name and other attributes.
	RakNet::BitStream sendBitstream;
	sendBitstream.Write((unsigned char)NMSG_ADD_PLAYER);
	sendBitstream.Write(name.c_str());
	sendBitstream.Write(player->GetId());

	// Send the message to all clients. ("PlayerName has connected to the game").
	mRaknetPeer->Send(&sendBitstream, HIGH_PRIORITY, RELIABLE_ORDERED, 0, RakNet::UNASSIGNED_SYSTEM_ADDRESS, true);
}

void Server::HandleNamesRequest(RakNet::BitStream& bitstream, RakNet::SystemAddress adress)
{
	// Send all client names back to the requested client.
	RakNet::BitStream sendBitstream;
	sendBitstream.Write((unsigned char)NSMG_CONNECTED_CLIENTS);
	sendBitstream.Write(mWorld->GetNumObjects(GLib::PLAYER));

	vector<string> clients;
	GLib::ObjectList* objects = mWorld->GetObjects();
	for(int i = 0; i < objects->size(); i++)
	{
		GLib::Object3D* object = objects->operator[](i);
		if(object->GetType() == GLib::PLAYER)
			sendBitstream.Write(object->GetName().c_str());
	}

	mRaknetPeer->Send(&sendBitstream, HIGH_PRIORITY, RELIABLE, 0, adress, false);
}

void Server::HandleSkillCasted(RakNet::BitStream& bitstream)
{
	unsigned char skillCasted;
	bitstream.Read(skillCasted);
	mSkillInterpreter->Interpret(this, (MessageId)skillCasted, bitstream);
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
			bitstream.Write(((Player*)object)->GetHealth());

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

void Server::SendClientMessage(RakNet::BitStream& bitstream)
{
	mRaknetPeer->Send(&bitstream, HIGH_PRIORITY, RELIABLE_ORDERED, 0, RakNet::UNASSIGNED_SYSTEM_ADDRESS, true);
}

RakNet::RakPeerInterface* Server::GetRaknetPeer()
{
	return mRaknetPeer;
}

GLib::World* Server::GetWorld()
{
	return mWorld;
}

