#pragma once
#include "RakPeerInterface.h"
#include "d3dUtil.h"
#include "States.h"
#include "ServerCvars.h"
#include "Database.h"
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
class ServerArena;
class Database;

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

	void SendClientMessage(RakNet::BitStream& bitstream, bool broadcast = true, RakNet::SystemAddress adress = RakNet::UNASSIGNED_SYSTEM_ADDRESS);
	void AddClientChatText(string text, COLORREF color);

	RakNet::RakPeerInterface*	GetRaknetPeer();
	vector<string>				GetConnectedClients();
	GLib::World*				GetWorld();
	RoundHandler*				GetRoundHandler();
	ServerSkillInterpreter*		GetSkillInterpreter();
	ItemLoaderXML*				GetItemLoader();
	CurrentState				GetArenaState();
	int							GetCvarValue(string cvar);

	void StartGame();
	void SetGameSate(CurrentState state);
	void DrawScores(GLib::Graphics* pGraphics);
	void SetScore(string name, int score);
	void AddScore(string name, int score);
	void AddRoundCompleted();
	void SetCvarValue(string cvar, int value);
	string RemovePlayer(RakNet::SystemAddress adress);

	bool IsHost(string name);
	bool IsCvarCommand(string cmd);
	bool IsRoundOver(string& winner);
	bool IsGameOver();
private:
	RakNet::RakPeerInterface*	mRaknetPeer;
	ServerSkillInterpreter*		mSkillInterpreter;
	ServerMessageHandler*		mMessageHandler;
	RoundHandler*				mRoundHandler;
	ItemLoaderXML*				mItemLoader;
	ServerArena*				mArena;
	ServerCvars					mCvars;

	Database*					mDatabase;
	string						mServerName;
	string						mHostName;

	map<string, int>			mScoreMap;
};