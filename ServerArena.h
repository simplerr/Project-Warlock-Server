#pragma once
#include <string>
#include <vector>
#include "BitStream.h"
using namespace std;

namespace GLib {
	class World;
	class Player;
	class Object3D;
	class Input;
	class Graphics;
}

class Server;
class Player;
class CollisionHandler;

class ServerArena
{
public:
	ServerArena(Server* pServer);
	~ServerArena();

	void Update(GLib::Input* pInput, float dt);
	void Draw(GLib::Graphics* pGraphics);
	void BroadcastWorld();

	void OnObjectAdded(GLib::Object3D* pObject);
	void OnObjectRemoved(GLib::Object3D* pObject);
	void OnObjectCollision(GLib::Object3D* pObjectA, GLib::Object3D* pObjectB);

	string	RemovePlayer(RakNet::SystemAddress adress);
	void	RemovePlayer(int id);
	void	PlayerEliminated(Player* pPlayer, Player* pEliminator);

	GLib::World* GetWorld();
	vector<Player*>* GetPlayerListPointer();
private:
	Server*				mServer;
	GLib::World*		mWorld;
	vector<Player*>		mPlayerList;
	CollisionHandler*	mCollisionHandler;
	float				mTickRate;
	float				mTickCounter;
	float				mDamageCounter;
};