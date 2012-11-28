#pragma once
#include "BitStream.h"
#include "NetworkMessages.h"

class Server;
class Client;

class ServerSkillInterpreter
{
public:
	ServerSkillInterpreter();
	~ServerSkillInterpreter();

	void Interpret(Server* pServer, MessageId id, RakNet::BitStream& bitstream);
private:
};