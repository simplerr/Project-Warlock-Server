#include "ServerMessageHandler.h"
#include "ServerArena.h"
#include "ServerSkillInterpreter.h"
#include "Server.h"
#include "World.h"
#include "Object3D.h"
#include "Player.h"
#include "RoundHandler.h"
#include "NetworkMessages.h"

ServerMessageHandler::ServerMessageHandler(Server* pServer)
{
	mServer = pServer;
}

ServerMessageHandler::~ServerMessageHandler()
{

}

void ServerMessageHandler::HandleNewConnection(RakNet::BitStream& bitstream, RakNet::SystemAddress adress)
{
	// Send connection successful message back.
	// The message contains the players already connected.
	GLib::World* world = mServer->GetWorld();
	RakNet::BitStream sendBitstream;
	sendBitstream.Write((unsigned char)NMSG_CONNECTION_SUCCESS);
	sendBitstream.Write(mServer->GetRoundHandler()->GetArenaState().state);
	sendBitstream.Write(world->GetNumObjects(GLib::PLAYER));

	GLib::ObjectList* objects = world->GetObjects();
	for(int i = 0; i < objects->size(); i++)
	{
		GLib::Object3D* object = objects->operator[](i);
		if(object->GetType() == GLib::PLAYER) {
			sendBitstream.Write(object->GetName().c_str());
			sendBitstream.Write(object->GetId());
			sendBitstream.Write(object->GetPosition());
		}
	}

	mServer->SendClientMessage(sendBitstream, false, adress);
}

void ServerMessageHandler::HandleConnectionLost(RakNet::BitStream& bitstream, RakNet::SystemAddress adress)
{
	RakNet::BitStream sendBitstream;

	string name = mServer->RemovePlayer(adress);
	sendBitstream.Write((unsigned char)NMSG_PLAYER_DISCONNECTED);
	sendBitstream.Write(name.c_str());	

	OutputDebugString(string(name + " has disconnected!").c_str());

	// Tell the other clients about the disconnect.
	mServer->SendClientMessage(sendBitstream);
}

void ServerMessageHandler::HandleTargetAdded(RakNet::BitStream& bitstream)
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

	GLib::ObjectList* objects = mServer->GetWorld()->GetObjects();
	for(int i = 0; i < objects->size(); i++)
	{
		Actor* actor = (Actor*)objects->operator[](i);
		if(actor->GetId() == id && !actor->IsKnockedBack()) {
			actor->AddTarget(XMFLOAT3(x, y, z), clear);

			// Send the TARGET_ADDED to all clients.
			mServer->SendClientMessage(bitstream);
			break;
		}
	}
}

void ServerMessageHandler::HandleConnectionData(RakNet::BitStream& bitstream, RakNet::SystemAddress adress)
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
	player->SetGold(mServer->GetCvarValue(Cvars::START_GOLD));
	mServer->GetWorld()->AddObject(player);

	OutputDebugString(string(name + " has connected!\n").c_str());

	// [TODO] Add model name and other attributes.
	RakNet::BitStream sendBitstream;
	sendBitstream.Write((unsigned char)NMSG_ADD_PLAYER);
	sendBitstream.Write(name.c_str());
	sendBitstream.Write(player->GetId());
	sendBitstream.Write(mServer->GetCvarValue(Cvars::START_GOLD));

	// Set starting score.
	mServer->SetScore(name, 0);

	// Send the message to all clients. ("PlayerName has connected to the game").
	mServer->SendClientMessage(sendBitstream);

	// Send cvar values.
	auto cvarMap = mServer->GetCvars().CvarMap;
	for(auto iter = cvarMap.begin(); iter != cvarMap.end(); iter++) {
		string cvar = (*iter).first;
		int value = (*iter).second;
		SendCvarValue(adress, cvar, value, true);
	}

	// [NOTE][TEMP] Start the round.
	//mServer->GetRoundHandler()->StartRound();
}

void ServerMessageHandler::HandleNamesRequest(RakNet::BitStream& bitstream, RakNet::SystemAddress adress)
{
	// Send all client names back to the requested client.
	GLib::World* world = mServer->GetWorld();
	RakNet::BitStream sendBitstream;
	sendBitstream.Write((unsigned char)NSMG_CONNECTED_CLIENTS);
	sendBitstream.Write(world->GetNumObjects(GLib::PLAYER));

	vector<string> clients;
	GLib::ObjectList* objects = world->GetObjects();
	for(int i = 0; i < objects->size(); i++)
	{
		GLib::Object3D* object = objects->operator[](i);
		if(object->GetType() == GLib::PLAYER)
			sendBitstream.Write(object->GetName().c_str());
	}

	mServer->SendClientMessage(sendBitstream, false, adress);
}

void ServerMessageHandler::HandleCvarListRequest(RakNet::BitStream& bitstream, RakNet::SystemAddress adress)
{
	// Send all client names back to the requested client.
	RakNet::BitStream sendBitstream;
	sendBitstream.Write((unsigned char)NMSG_REQUEST_CVAR_LIST);
	sendBitstream.Write(mServer->GetCvarValue(Cvars::START_GOLD));
	sendBitstream.Write(mServer->GetCvarValue(Cvars::SHOP_TIME));
	sendBitstream.Write(mServer->GetCvarValue(Cvars::ROUND_TIME));
	sendBitstream.Write(mServer->GetCvarValue(Cvars::NUM_ROUNDS));
	sendBitstream.Write(mServer->GetCvarValue(Cvars::GOLD_PER_KILL));
	sendBitstream.Write(mServer->GetCvarValue(Cvars::GOLD_PER_WIN));
	sendBitstream.Write(mServer->GetCvarValue(Cvars::LAVA_DMG));
	sendBitstream.Write(mServer->GetCvarValue(Cvars::PROJECTILE_IMPULSE));

	mServer->SendClientMessage(sendBitstream, false, adress);
}

void ServerMessageHandler::HandleSkillCasted(RakNet::BitStream& bitstream)
{
	unsigned char skillCasted;
	bitstream.Read(skillCasted);
	mServer->GetSkillInterpreter()->Interpret(mServer, (MessageId)skillCasted, bitstream);
}

void ServerMessageHandler::HandleItemAdded(RakNet::BitStream& bitstream, RakNet::SystemAddress adress)
{
	ItemName name;
	int playerId, level;
	bitstream.Read(playerId);
	bitstream.Read(name);
	bitstream.Read(level);

	Player* player = (Player*)mServer->GetWorld()->GetObjectById(playerId);
	player->AddItem(mServer->GetItemLoader(), ItemKey(name, level));

	// Send to all client except to the one it came from.
	RakNet::BitStream sendBitstream;
	sendBitstream.Write((unsigned char)NMSG_ITEM_ADDED);
	sendBitstream.Write(playerId);
	sendBitstream.Write(name);
	sendBitstream.Write(level);

	mServer->SendClientMessage(sendBitstream, true, adress);
}

void ServerMessageHandler::HandleItemRemoved(RakNet::BitStream& bitstream, RakNet::SystemAddress adress)
{
	ItemName name;
	int playerId, level;
	bitstream.Read(playerId);
	bitstream.Read(name);
	bitstream.Read(level);

	Player* player = (Player*)mServer->GetWorld()->GetObjectById(playerId);
	player->RemoveItem(mServer->GetItemLoader()->GetItem(ItemKey(name, level)));

	// [TODO] REMOVE SKILLS!! [TODO]

	// Send to all client except to the one it came from.
	RakNet::BitStream sendBitstream;
	sendBitstream.Write((unsigned char)NMSG_ITEM_REMOVED);
	sendBitstream.Write(playerId);
	sendBitstream.Write(name);
	sendBitstream.Write(level);

	mServer->SendClientMessage(sendBitstream, true, adress);
}

void ServerMessageHandler::HandleGoldChange(RakNet::BitStream& bitstream, RakNet::SystemAddress adress)
{
	int id, gold;
	bitstream.Read(id);
	bitstream.Read(gold);

	((Player*)mServer->GetWorld()->GetObjectById(id))->SetGold(gold);
}

void ServerMessageHandler::HandleChatMessage(RakNet::BitStream& bitstream, RakNet::SystemAddress adress)
{
	// Send the message to all clients.
	mServer->SendClientMessage(bitstream);

	// CVAR command?
	char from[32];
	char message[256];

	bitstream.Read(from);
	bitstream.Read(message);

	string msg = string(message).substr(0, string(message).size() - 2);
	vector<string> elems = GLib::SplitString(msg, ' ');
	if(mServer->IsHost(from) && mServer->IsCvarCommand(elems[0]))
	{
		if(elems[0] == Cvars::RESTART_ROUND)
			mServer->GetRoundHandler()->StartRound();
		else if(elems[0] == Cvars::GIVE_GOLD && elems.size() == 3) // -gold playername amount
		{
			Player* target = (Player*)mServer->GetWorld()->GetObjectByName(elems[1]);
			if(target != nullptr && elems[2].find_first_not_of("0123456789") == std::string::npos) {
				int gold = atoi(elems[2].c_str());
				target->SetGold(target->GetGold() + gold);

				mServer->AddClientChatText("The host gave " + elems[2] + " gold to " + target->GetName() + "\n", RGB(0, 200, 0));
			}
		}
		else if(elems.size() == 2 && !elems[1].empty() && elems[1].find_first_not_of("0123456789") == std::string::npos)
		{
			int value = atoi(elems[1].c_str());

			// Send cvar change message.
			mServer->SetCvarValue(elems[0], value);
			SendCvarValue(adress, elems[0], value, true);
		}
	}
}

void ServerMessageHandler::HandleRematchRequest(RakNet::BitStream& bitstream)
{
	// Remove scores and reset the round handler.
	mServer->ResetScores();
	mServer->GetRoundHandler()->StartRound();
	mServer->GetRoundHandler()->Rematch();
	mServer->GetArena()->StartGame();

	// Inform all clients about the rematch.
	RakNet::BitStream sendBitstream;
	sendBitstream.Write((unsigned char)NMSG_PERFORM_REMATCH);
	mServer->SendClientMessage(sendBitstream);
}

void ServerMessageHandler::SendCvarValue(RakNet::SystemAddress adress, string cvar, int value, bool show)
{
	// Send cvar change message.
	RakNet::BitStream bitstream;
	bitstream.Write((unsigned char)NMSG_CVAR_CHANGE);
	bitstream.Write(cvar.c_str());
	bitstream.Write(value);
	bitstream.Write(show ? 1 : 0); // Show change in chat or not.
	mServer->SendClientMessage(bitstream);
}