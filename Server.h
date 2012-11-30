#pragma once
#include "RakPeerInterface.h"
#include "d3dUtil.h"

namespace GLib {
	class Input;
	class Graphics;
	class Object3D;
	class World;
}

class ServerSkillInterpreter;
class CollisionHandler;
class Player;
class ItemLoaderXML;

class Server
{
public:
	Server();
	~Server();

	void Update(GLib::Input* pInput, float dt);
	void Draw(GLib::Graphics* pGraphics);
	bool StartServer();
	bool ListenForPackets();
	bool HandlePacket(RakNet::Packet* pPacket);
	string RemovePlayer(RakNet::SystemAddress adress);
	void RemovePlayer(int id);

	void OnObjectAdded(GLib::Object3D* pObject);
	void OnObjectRemoved(GLib::Object3D* pObject);
	void OnObjectCollision(GLib::Object3D* pObjectA, GLib::Object3D* pObjectB);

	void SendClientMessage(RakNet::BitStream& bitstream);
	void BroadcastWorld();

	RakNet::RakPeerInterface* GetRaknetPeer();
	vector<string> GetConnectedClients();
	GLib::World* GetWorld();

	//
	// Handle packet functions.
	//
	void HandleNewConnection(RakNet::BitStream& bitstream, RakNet::SystemAddress adress);
	void HandleConnectionLost(RakNet::BitStream& bitstream, RakNet::SystemAddress adress);
	void HandleConnectionData(RakNet::BitStream& bitstream, RakNet::SystemAddress adress);
	void HandleNamesRequest(RakNet::BitStream& bitstream, RakNet::SystemAddress adress);
	void HandleItemAdded(RakNet::BitStream& bitstream, RakNet::SystemAddress adress);
	void HandleItemRemoved(RakNet::BitStream& bitstream, RakNet::SystemAddress adress);
	void HandleGoldChange(RakNet::BitStream& bitstream, RakNet::SystemAddress adress);
	void HandleTargetAdded(RakNet::BitStream& bitstream);
	void HandleSkillCasted(RakNet::BitStream& bitstream);

private:
	RakNet::RakPeerInterface*	mRaknetPeer;
	GLib::World*				mWorld;
	ServerSkillInterpreter*		mSkillInterpreter;
	CollisionHandler*			mCollisionHandler;
	vector<Player*>				mPlayerList;
	Player*						mTestDoll;
	ItemLoaderXML*				mItemLoader;
	float						mTickRate;
	float						mTickCounter;
	float						mDamageCounter;
};