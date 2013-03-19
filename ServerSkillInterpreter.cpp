#include "ServerSkillInterpreter.h"
#include "Server.h"
#include "World.h"
#include "Skills.h"
#include "Items.h"
#include "FrostProjectile.h"
#include "FireProjectile.h"
#include "MeteorProjectile.h"
#include "HookProjectile.h"
#include "VenomProjectile.h"
#include "Player.h"
#include "Console.h"

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

	Player* player = (Player*)world->GetObjectById(owner);
	Projectile* projectile = nullptr;

	// Set player rotation facing dir target.
	player->SetRotation(XMFLOAT3(0, atan2f(-dir.x, -dir.z), 0));

	// Set attack animation.
	player->SetAnimation(5, 0.7f);

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
		teleport.Cast(world, (Player*)player, start, end);
	}
	else if(id == SKILL_METEOR)
	{
		projectile = new MeteorProjectile(owner, end);
	}
	else if(id == SKILL_VENOM) 
	{
		projectile = new VenomProjectile(owner, start, dir);
	}

	RakNet::BitStream sendBitstream;
	sendBitstream.Write((unsigned char)NMSG_SKILL_CAST);
	sendBitstream.Write((unsigned char)id);
	sendBitstream.Write(owner);
	sendBitstream.Write(skillType);
	sendBitstream.Write(skillLevel);
	sendBitstream.Write(start);
	sendBitstream.Write(end);

	if(id == SKILL_FIREBALL || id == SKILL_FROSTNOVA || id == SKILL_METEOR || id == SKILL_HOOK || id == SKILL_VENOM)
	{
		world->AddObject(projectile);
		projectile->SetSkillLevel(skillLevel);
		projectile->SetSkillType(skillType);

		projectile->SetPosition(projectile->GetPosition() + XMFLOAT3(0, 2, 0));
		sendBitstream.Write(projectile->GetId());
	}
	char buffer[10];
	sprintf(buffer, "(%i)", skillType);
	gConsole->AddLine("[" + player->GetName() + "] CAST_SKILL " + buffer);

	// Send it to all the clients.
	pServer->SendClientMessage(sendBitstream);
}