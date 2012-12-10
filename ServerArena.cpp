#include "ServerArena.h"
#include "CollisionHandler.h"
#include "Server.h"
#include "World.h"
#include "Object3D.h"
#include "Projectile.h"
#include "NetworkMessages.h"
#include "BitStream.h"
#include "Player.h"
#include "d3dUtil.h"
#include "RoundHandler.h"

ServerArena::ServerArena(Server* pServer)
{
	mServer = pServer;

	// Create the world.
	mWorld = new GLib::World();
	mWorld->Init(GLib::GetGraphics());

	mWorld->AddObjectAddedListener(&ServerArena::OnObjectAdded, this);
	mWorld->AddObjectRemovedListener(&ServerArena::OnObjectRemoved, this);
	mWorld->AddObjectCollisionListener(&ServerArena::OnObjectCollision, this);

	// Connect the graphics light list.
	GLib::GetGraphics()->SetLightList(mWorld->GetLights());

	mCollisionHandler = new CollisionHandler();

	mTickRate = 1.0f / 100.0f;	// 100 ticks per second.
	mTickCounter = 0.0f;
	mDamageCounter = 0.0f;

	/************************************************************************/
	/* Add a test player.                                                   */
	/************************************************************************/
	Player* testDoll = new Player();
	testDoll->SetPosition(XMFLOAT3(0, 0, 10));
	testDoll->SetScale(XMFLOAT3(0.1f, 0.1f, 0.1f));	// [NOTE]
	mWorld->AddObject(testDoll);
}

ServerArena::~ServerArena()
{
	delete mWorld;
	delete mCollisionHandler;
}

void ServerArena::Update(GLib::Input* pInput, float dt)
{
	mWorld->Update(dt);

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
				mPlayerList[i]->SetHealth(mPlayerList[i]->GetHealth() - mServer->GetCvarValue(Cvars::LAVA_DMG));

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
		if(mServer->IsRoundOver(winner))
		{
			// Add gold to the winner.
			Player* winningPlayer = (Player*)mWorld->GetObjectByName(winner);
			winningPlayer->SetGold(winningPlayer->GetGold() + mServer->GetCvarValue(Cvars::GOLD_PER_WIN));

			// Send round ended message.
			RakNet::BitStream bitstream;
			bitstream.Write((unsigned char)NMSG_ROUND_ENDED);
			bitstream.Write(winner.c_str());
			mServer->SendClientMessage(bitstream);

			// Increment winners score.
			mServer->AddScore(winner, 1);
		}

		BroadcastWorld();
		mServer->GetRoundHandler()->BroadcastStateTimer();
		mTickCounter = 0.0f;
	}
}

void ServerArena::Draw(GLib::Graphics* pGraphics)
{
	mWorld->Draw(pGraphics);
}

void ServerArena::BroadcastWorld()
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

		mServer->SendClientMessage(bitstream);
	}
}

//! Gets called in World::AddObject().
void ServerArena::OnObjectAdded(GLib::Object3D* pObject)
{
	// Add player to mPlayerList.
	if(pObject->GetType() == GLib::PLAYER)
		mPlayerList.push_back((Player*)pObject);
}

//! Gets called in World::RemoveObject().
void ServerArena::OnObjectRemoved(GLib::Object3D* pObject)
{
	// Remove player from mPlayerList:
	if(pObject->GetType() == GLib::PLAYER) 
		RemovePlayer(pObject->GetId());

	RakNet::BitStream bitstream;
	bitstream.Write((unsigned char)NMSG_OBJECT_REMOVED);
	bitstream.Write(pObject->GetId());
	mServer->SendClientMessage(bitstream);
}

void ServerArena::OnObjectCollision(GLib::Object3D* pObjectA, GLib::Object3D* pObjectB)
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

		if(mServer->GetArenaState() != SHOPPING_STATE)
		{
			// Checks what skill the projectile is and uses the XML data to determine the skill attributes
			// depending on the skill level.
			mCollisionHandler->HandleCollision(player, projectile, this, mServer->GetItemLoader(), mServer->GetCvarValue(Cvars::PROJECTILE_IMPULSE) / 10.0f);

			// Let the clients now about the changes immediately.
			BroadcastWorld();

			// Tell all clients about the collision.
			RakNet::BitStream bitstream;
			bitstream.Write((unsigned char)NMSG_PROJECTILE_PLAYER_COLLISION);
			bitstream.Write(projectile->GetId());	// Projectile Id.
			bitstream.Write(player->GetId());	// Player Id.
			mServer->SendClientMessage(bitstream);
		}

		// Remove the projectile.
		mWorld->RemoveObject(projectile);
	}
}

void ServerArena::PlayerEliminated(Player* pKilled, Player* pEliminator)
{
	// Add gold to the killer. 
	if(pEliminator != nullptr)
		pEliminator->SetGold(pEliminator->GetGold() + mServer->GetCvarValue(Cvars::GOLD_PER_KILL));

	// Tell the clients about the kill.
	RakNet::BitStream bitstream;
	bitstream.Write((unsigned char)NMSG_PLAYER_ELIMINATED);
	bitstream.Write(pKilled->GetName().c_str());
	bitstream.Write(pEliminator == nullptr ? "himself" : pEliminator->GetName().c_str());
	mServer->SendClientMessage(bitstream);
}

//! Removes a player from mPlayerList.
void ServerArena::RemovePlayer(int id)
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

string ServerArena::RemovePlayer(RakNet::SystemAddress adress)
{
	string name = "#NOVALUE";

	for(auto iter =  mPlayerList.begin(); iter != mPlayerList.end(); iter++)
	{
		if((*iter)->GetSystemAdress() == adress) {
			name = (*iter)->GetName();
			mWorld->RemoveObject((*iter)->GetId());
			break;
		}
	}

	return name;
}

GLib::World* ServerArena::GetWorld()
{
	return mWorld;
}

vector<Player*>* ServerArena::GetPlayerListPointer()
{
	return &mPlayerList;
}