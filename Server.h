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
class ServerMessageHandler;
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

	void SendClientMessage(RakNet::BitStream& bitstream, bool broadcast = true, RakNet::SystemAddress adress = RakNet::UNASSIGNED_SYSTEM_ADDRESS);
	void AddClientChatText(string text, COLORREF color);
	void BroadcastWorld();

	RakNet::RakPeerInterface*	GetRaknetPeer();
	vector<string>				GetConnectedClients();
	GLib::World*				GetWorld();
	RoundHandler*				GetRoundHandler();
	ServerSkillInterpreter*		GetSkillInterpreter();
	ItemLoaderXML*				GetItemLoader();
	int							GetCvarValue(string cvar);

	void SetGameSate(GameState state);
	void DrawScores(GLib::Graphics* pGraphics);
	void SetScore(string name, int score);
	void SetCvarValue(string cvar, int value);

	bool IsHost(string name);
	bool IsCvarCommand(string cmd);
private:
	RakNet::RakPeerInterface*	mRaknetPeer;
	GLib::World*				mWorld;
	ServerSkillInterpreter*		mSkillInterpreter;
	ServerMessageHandler*		mMessageHandler;
	CollisionHandler*			mCollisionHandler;
	RoundHandler*				mRoundHandler;
	vector<Player*>				mPlayerList;
	Player*						mTestDoll;
	ItemLoaderXML*				mItemLoader;
	ServerCvars					mCvars;
	float						mTickRate;
	float						mTickCounter;
	float						mDamageCounter;
	
	map<string, int>			mScoreMap;
};