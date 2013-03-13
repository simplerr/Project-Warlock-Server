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
#include "Camera.h"
#include "ItemLoaderXML.h"

ServerArena::ServerArena(Server* pServer)
	: BaseArena()
{
	mServer = pServer;

	mWorld->AddObjectAddedListener(&ServerArena::OnObjectAdded, this);
	mWorld->AddObjectRemovedListener(&ServerArena::OnObjectRemoved, this);
	mWorld->AddObjectCollisionListener(&ServerArena::OnObjectCollision, this);

	mCollisionHandler = new CollisionHandler();

	mTickRate = 1.0f / 60.0f;	// 100 ticks per second.
	mTickCounter = 0.0f;
	mDamageCounter = 0.0f;

	mGameStarted = false;

	/************************************************************************/
	/* Add a test player.                                                   */
	/************************************************************************/
	Player* testDoll = new Player();
	testDoll->SetPosition(XMFLOAT3(0, 0, 10));
	testDoll->SetScale(XMFLOAT3(0.1f, 0.1f, 0.1f));	// [NOTE]
	testDoll->AddItem(pServer->GetItemLoader(), ItemKey(KNOCKBACK_SHIELD, 3));
	mWorld->AddObject(testDoll);

	GLib::GetGraphics()->GetCamera()->SetPosition(XMFLOAT3(0, 150, 30));
	GLib::GetGraphics()->GetCamera()->SetTarget(XMFLOAT3(0, 0, 0));
}

ServerArena::~ServerArena()
{
	delete mCollisionHandler;
}

void ServerArena::Update(GLib::Input* pInput, float dt)
{
	if(!IsGameStarted())
		return;

	mWorld->Update(dt);
	
	mDamageCounter += dt;

	for(int i = 0; i < mPlayerList.size(); i++) 
	{
		if(mPlayerList[i]->GetEliminated())
			continue;

		// Deal damage to players outside the arena.
		if(mDamageCounter > 0.1f)
		{
			XMFLOAT3 pos = mPlayerList[i]->GetPosition();
			float distFromCenter = sqrt(pos.x * pos.x + pos.z * pos.z);

			// Arena is 60 units in radius.
			if(distFromCenter > 60.0f) {
				mPlayerList[i]->TakeDamage(mServer->GetCvarValue(Cvars::LAVA_DMG) * (1 - mPlayerList[i]->GetLavaImmunity()));

				// Set slow movement speed.
				mPlayerList[i]->SetSlow(mServer->GetCvarValue(Cvars::LAVA_SLOW));
			}
			else {
				// Restore movement speed.
				mPlayerList[i]->SetSlow(0.0f);
			}
		}
		
		// Player dead?
		if(mPlayerList[i]->GetCurrentHealth() <= 0 && mPlayerList[i]->GetCurrentAnimation() != 7) {
			mPlayerList[i]->SetDeathAnimation();
			PlayerEliminated(mPlayerList[i], mPlayerList[i]->GetLastHitter());
		}
	}

	if(mDamageCounter > 0.1f)
		mDamageCounter = 0.0f;

	// Broadcast the world at a fixed rate.
	mTickCounter += dt;
	if(mTickCounter > mTickRate) {
		// Find out if there is only 1 player alive (round ended).
		string winner;
		if(mServer->IsRoundOver(winner))
		{
			mServer->AddRoundCompleted();

			// Add gold to the winner.
			Player* winningPlayer = (Player*)mWorld->GetObjectByName(winner);
			winningPlayer->SetGold(winningPlayer->GetGold() + mServer->GetCvarValue(Cvars::GOLD_PER_WIN));

			RakNet::BitStream bitstream;
			if(mServer->IsGameOver())
				bitstream.Write((unsigned char)NMSG_GAME_OVER);
			else 
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
	if(IsGameStarted())
		mWorld->Draw(pGraphics);
	else
		pGraphics->DrawText("Players in lobby", 10, 200, 20);
}

void ServerArena::StartGame()
{
	mGameStarted = true;
}

void ServerArena::BroadcastWorld()
{
	// Broadcast world data at a set tickrate.
	GLib::ObjectList* objects = mWorld->GetObjects();
	for(int i = 0; i < objects->size(); i++)
	{
		GLib::Object3D* object = (GLib::Object3D*)objects->operator[](i);
		XMFLOAT3 pos = object->GetPosition();
		XMFLOAT3 rotation = object->GetRotation();

		RakNet::BitStream bitstream;
		bitstream.Write((unsigned char)NMSG_WORLD_UPDATE);
		bitstream.Write(object->GetType());
		bitstream.Write((unsigned char)object->GetId());
		bitstream.Write(pos.x);
		bitstream.Write(pos.y);
		bitstream.Write(pos.z);
		bitstream.Write(rotation.x);
		bitstream.Write(rotation.y);
		bitstream.Write(rotation.z);

		if(object->GetType() == GLib::PLAYER)
		{
			Player* player = (Player*)object;
			bitstream.Write(player->GetCurrentAnimation());
			bitstream.Write(player->GetDeathTimer());
			bitstream.Write(player->GetCurrentHealth());
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

		if(mServer->GetArenaState() != SHOPPING_STATE && !player->GetEliminated())
		{
			// Checks what skill the projectile is and uses the XML data to determine the skill attributes
			// depending on the skill level.
			projectile->HandlePlayerCollision(player, this, mServer->GetItemLoader());

			// Add status effect if there is any.
			StatusEffect* statusEffect = projectile->GetStatusEffect();
			if(statusEffect != nullptr)
				player->AddStatusEffect(statusEffect);

			// Set hurt animation.
			player->SetAnimation(6, 0.4f);

			// Add lifesteal life.
			Player* owner = (Player*)mWorld->GetObjectById(projectile->GetOwner());
			owner->SetCurrentHealth(owner->GetCurrentHealth() + owner->GetLifeSteal());

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

bool ServerArena::IsGameStarted()
{
	return mGameStarted;
}