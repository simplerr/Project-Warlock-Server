#include "ServerSkillInterpreter.h"
#include "Server.h"
#include "World.h"
#include "Skills.h"
#include "Items.h"
#include "FrostProjectile.h"
#include "FireProjectile.h"
#include "MeteorProjectile.h"
#include "HookProjectile.h"

ServerSkillInterpreter::ServerSkillInterpreter()
{

}

ServerSkillInterpreter::~ServerSkillInterpreter()
{

}

void ServerSkillInterpreter::Interpret(Server* pServer, MessageId id, RakNet::BitStream& bitstream)
{
	GLib::World* world = pServer->GetWorld();

	XMFLOAT3 start, end, dir;
	int owner, skillLevel;
	ItemName skillType;

	bitstream.Read(owner);
	bitstream.Read(skillType);
	bitstream.Read(skillLevel);
	bitstream.Read(start);
	bitstream.Read(end);

	XMStoreFloat3(&dir, XMVector3Normalize(XMLoadFloat3(&end) - XMLoadFloat3(&start)));

	Projectile* projectile = nullptr;

	if(id == SKILL_FIREBALL)
		projectile = new FireProjectile(owner, start, dir);
	else if(id == SKILL_FROSTNOVA)
		projectile = new FrostProjectile(owner, start);
	else if(id == SKILL_HOOK)
		projectile = new HookProjectile(owner, start, dir);
	else if(id == SKILL_TELEPORT) 
	{
		// This is really ugly, should be handled by Teleport itself...
		Teleport teleport;
		GLib::Object3D* player = world->GetObjectById(owner);
		teleport.Cast(world, (Player*)player, start, end);
	}
	else if(id == SKILL_METEOR)
	{
		projectile = new MeteorProjectile(owner, end);
	}

	RakNet::BitStream sendBitstream;
	sendBitstream.Write((unsigned char)NMSG_SKILL_CAST);
	sendBitstream.Write((unsigned char)id);
	sendBitstream.Write(owner);
	sendBitstream.Write(skillType);
	sendBitstream.Write(skillLevel);
	sendBitstream.Write(start);
	sendBitstream.Write(end);

	if(id == SKILL_FIREBALL || id == SKILL_FROSTNOVA || id == SKILL_METEOR || id == SKILL_HOOK)
	{
		world->AddObject(projectile);
		projectile->SetSkillLevel(skillLevel);
		projectile->SetSkillType(skillType);

		sendBitstream.Write(projectile->GetId());
	}
	

	// Send it to all the clients.
	pServer->SendClientMessage(sendBitstream);
}