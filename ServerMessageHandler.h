#pragma once
#include "BitStream.h"
#include <string>

using namespace std;

class Server;

class ServerMessageHandler
{
public:
	ServerMessageHandler(Server* pServer);
	~ServerMessageHandler();

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
	void HandleRematchRequest(RakNet::BitStream& bitstream);
	void HandleChatMessage(RakNet::BitStream& bitstream, RakNet::SystemAddress adress);

	void SendCvarValue(RakNet::SystemAddress adress, string cvar, int value, bool show);
private:
	Server* mServer;
};