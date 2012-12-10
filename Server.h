#pragma once
#include "RakPeerInterface.h"
#include "d3dUtil.h"
#include "States.h"
#include "ServerCvars.h"
#include <string>
#include <map>

using namespace std;

namespace GLib {
	class Input;
	class Graphics;
	class Object3D;
	class World;
}

class ServerSkillInterpreter;
class CollisionHandler;
class RoundHandler;
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

	void PlayerEliminated(Player* pPlayer, Player* pEliminator);

	void SendClientMessage(RakNet::BitStream& bitstream);
	void AddClientChatText(string text, COLORREF color);
	void BroadcastWorld();

	RakNet::RakPeerInterface*	GetRaknetPeer();
	vector<string>				GetConnectedClients();
	GLib::World*				GetWorld();
	int							GetCvarValue(string cvar);

	void SetGameSate(GameState state);

	//
	// Handle packet functions.
	//
	void HandleNewConnection(RakNet::BitStream& bitstream, RakNet::SystemAddress adress);
	void HandleConnectionLost(RakNet::BitStream& bitstream, RakNet::SystemAddress adress);
	void HandleConnectionData(RakNet::BitStream& bitstream, RakNet::SystemAddress adress);
	void HandleNamesRequest(RakNet::BitStream& bitstream, RakNet::SystemAddress adress);
	void HandleCvarListRequest(RakNet::BitStream& bitstream, RakNet::SystemAddress adress);
	void HandleItemAdded(RakNet::BitStream& bitstream, RakNet::SystemAddress adress);
	void HandleItemRemoved(RakNet::BitStream& bitstream, RakNet::SystemAddress adress);
	void HandleGoldChange(RakNet::BitStream& bitstream, RakNet::SystemAddress adress);
	void HandleTargetAdded(RakNet::BitStream& bitstream);
	void HandleSkillCasted(RakNet::BitStream& bitstream);
	void HandleChatMessage(RakNet::BitStream& bitstream);

	void DrawScores(GLib::Graphics* pGraphics);

	bool IsHost(string name);
	bool IsCvarCommand(string cmd);
private:
	RakNet::RakPeerInterface*	mRaknetPeer;
	GLib::World*				mWorld;
	ServerSkillInterpreter*		mSkillInterpreter;
	CollisionHandler*			mCollisionHandler;
	vector<Player*>				mPlayerList;
	Player*						mTestDoll;
	ItemLoaderXML*				mItemLoader;
	ServerCvars					mCvars;
	float						mTickRate;
	float						mTickCounter;
	float						mDamageCounter;

	RoundHandler*				mRoundHandler;
	map<string, int>			mScoreMap;
};