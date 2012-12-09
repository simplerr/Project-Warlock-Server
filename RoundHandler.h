#pragma once
#include "States.h"
#include <string>
#include <vector>

using namespace std;

namespace GLib {
	class Graphics;
	class Input;
}

class Player;
class Server;

class RoundHandler
{
public:
	RoundHandler();
	~RoundHandler();

	void Update(GLib::Input* pInput, float dt);
	void Draw(GLib::Graphics* pGraphics);

	void StartRound();
	bool HasRoundEnded(string& winner);
	void BroadcastStateTimer();

	void SetPlayerList(vector<Player*>* pPlayerList);
	void SetServer(Server* pServer);
	ArenaState GetArenaState();
private:
	vector<Player*>* mPlayerList;
	ArenaState		 mArenaState;
	Server*			 mServer;
	bool			 mRoundEnded;
	float			 mEndCounter;
};