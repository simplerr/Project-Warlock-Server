#include "ServerSkillInterpreter.h"
#include "Server.h"
#include "World.h"
#include "Projectile.h"
#include "Skills.h"
#include "Items.h"

ServerSkillInterpreter::ServerSkillInterpreter()
{

}

ServerSkillInterpreter::~ServerSkillInterpreter()
{

}

void ServerSkillInterpreter::Interpret(Server* pServer, MessageId id, RakNet::BitStream& bitstream)
{
	GLib::World* world = pServer->GetWorld();

	if(id == NMSG_ADD_FIREBALL)
	{
		XMFLOAT3 start, end, dir;
		int owner, skillLevel;
		ItemName skillType;

		bitstream.Read(owner);
		bitstream.Read(skillType);
		bitstream.Read(skillLevel);
		bitstream.Read(start);
		bitstream.Read(end);

		XMStoreFloat3(&dir, XMVector3Normalize(XMLoadFloat3(&end) - XMLoadFloat3(&start)));
		Projectile* projectile = new Projectile(owner, start, dir);
		world->AddObject(projectile);
		projectile->SetSkillLevel(skillLevel);
		projectile->SetSkillType(skillType);

		// Send it to all the clients.
		RakNet::BitStream bitstream;
		bitstream.Write((unsigned char)NMSG_SKILL_CAST);
		bitstream.Write((unsigned char)NMSG_ADD_FIREBALL);
		bitstream.Write(owner);
		bitstream.Write(projectile->GetId());
		bitstream.Write(skillType);
		bitstream.Write(skillLevel);
		bitstream.Write(start);
		bitstream.Write(end);

		pServer->SendClientMessage(bitstream);
	}
}