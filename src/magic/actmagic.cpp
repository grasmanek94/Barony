/*-------------------------------------------------------------------------------

	BARONY
	File: actmagic.cpp
	Desc: behavior function for magic balls

	Copyright 2013-2016 (c) Turning Wheel LLC, all rights reserved.
	See LICENSE for details.

-------------------------------------------------------------------------------*/

#include "../main.hpp"
#include "../game.hpp"
#include "../stat.hpp"
#include "../entity.hpp"
#include "../interface/interface.hpp"
#include "../sound.hpp"
#include "../items.hpp"
#include "../monster.hpp"
#include "../net.hpp"
#include "../collision.hpp"
#include "../paths.hpp"
#include "../player.hpp"
#include "magic.hpp"

void actMagiclightBall(Entity* my)
{
	Entity* caster = NULL;
	if (!my)
	{
		return;
	}

	my->skill[2] = -10; // so the client sets the behavior of this entity

	if (clientnum != 0 && multiplayer == CLIENT)
	{
		my->removeLightField();

		//Light up the area.
		my->light = lightSphereShadow(my->x / 16, my->y / 16, 8, 192);
		lightball_flicker++;

		if (lightball_flicker > 5)
		{
			lightball_lighting = (lightball_lighting == 1) + 1;

			if (lightball_lighting == 1)
			{
				my->removeLightField();
				my->light = lightSphereShadow(my->x / 16, my->y / 16, 8, 192);
			}
			else
			{
				my->removeLightField();
				my->light = lightSphereShadow(my->x / 16, my->y / 16, 8, 174);
			}
			lightball_flicker = 0;
		}

		lightball_timer--;
		return;
	}

	my->yaw += .01;
	if ( my->yaw >= PI * 2 )
	{
		my->yaw -= PI * 2;
	}

	/*if (!my->parent) { //This means that it doesn't have a caster. In other words, magic light staff.
		return;
	})*/

	//list_t *path = NULL;
	pathnode_t* pathnode = NULL;

	//TODO: Follow player around (at a distance -- and delay before starting to follow).
	//TODO: Circle around player's head if they stand still for a little bit. Continue circling even if the player walks away -- until the player is far enough to trigger move (or if the player moved for a bit and then stopped, then update position).
	//TODO: Don't forget to cast flickering light all around it.
	//TODO: Move out of creatures' way if they collide.

	/*if (!my->children) {
		list_RemoveNode(my->mynode); //Delete the light spell.
		return;
	}*/
	if (!my->children.first)
	{
		list_RemoveNode(my->mynode); //Delete the light spell.C
		return;
	}
	node_t* node = NULL;

	spell_t* spell = NULL;
	node = my->children.first;
	spell = (spell_t*)node->element;
	if (!spell)
	{
		list_RemoveNode(my->mynode);
		return; //We need the source spell!
	}

	caster = uidToEntity(spell->caster);
	if (caster)
	{
		Stat* stats = caster->getStats();
		if (stats)
		{
			if (stats->HP <= 0)
			{
				my->removeLightField();
				list_RemoveNode(my->mynode); //Delete the light spell.
				return;
			}
		}
	}
	else if (spell->caster >= 1)     //So no caster...but uidToEntity returns NULL if entity is already dead, right? And if the uid is supposed to point to an entity, but it doesn't...it means the caster has died.
	{
		my->removeLightField();
		list_RemoveNode(my->mynode);
		return;
	}

	// if the spell has been unsustained, remove it
	if ( !spell->magicstaff && !spell->sustain )
	{
		int i = 0;
		int player = -1;
		for (i = 0; i < 4; ++i)
		{
			if (players[i]->entity == caster)
			{
				player = i;
			}
		}
		if (player > -1 && multiplayer == SERVER)
		{
			strcpy( (char*)net_packet->data, "UNCH");
			net_packet->data[4] = player;
			SDLNet_Write32(spell->ID, &net_packet->data[5]);
			net_packet->address.host = net_clients[player - 1].host;
			net_packet->address.port = net_clients[player - 1].port;
			net_packet->len = 9;
			sendPacketSafe(net_sock, -1, net_packet, player - 1);
		}
		my->removeLightField();
		list_RemoveNode(my->mynode);
		return;
	}

	if (magic_init)
	{
		my->removeLightField();

		if (lightball_timer <= 0)
		{
			if ( spell->sustain )
			{
				//Attempt to sustain the magic light.
				if (caster)
				{
					//Deduct mana from caster. Cancel spell if not enough mana (simply leave sustained at false).
					bool deducted = caster->safeConsumeMP(1); //Consume 1 mana every duration / mana seconds
					if (deducted)
					{
						lightball_timer = spell->channel_duration / getCostOfSpell(spell);
					}
					else
					{
						int i = 0;
						int player = -1;
						for (i = 0; i < 4; ++i)
						{
							if (players[i]->entity == caster)
							{
								player = i;
							}
						}
						if (player > -1 && multiplayer == SERVER)
						{
							strcpy( (char*)net_packet->data, "UNCH");
							net_packet->data[4] = player;
							SDLNet_Write32(spell->ID, &net_packet->data[5]);
							net_packet->address.host = net_clients[player - 1].host;
							net_packet->address.port = net_clients[player - 1].port;
							net_packet->len = 9;
							sendPacketSafe(net_sock, -1, net_packet, player - 1);
						}
						my->removeLightField();
						list_RemoveNode(my->mynode);
						return;
					}
				}
			}
		}

		//TODO: Make hovering always smooth. For example, when transitioning from ceiling to no ceiling, don't just have it jump to a new position. Figure out  away to transition between the two.
		if (lightball_hoverangle > 360)
		{
			lightball_hoverangle = 0;
		}
		if (map.tiles[(int)((my->y / 16) * MAPLAYERS + (my->x / 16) * MAPLAYERS * map.height)])
		{
			//Ceiling.
			my->z = lightball_hover_basez + ((lightball_hover_basez + LIGHTBALL_HOVER_HIGHPEAK + lightball_hover_basez + LIGHTBALL_HOVER_LOWPEAK) / 2) * sin(lightball_hoverangle * (12.568f / 360.0f)) * 0.1f;
		}
		else
		{
			//No ceiling. //TODO: Float higher?
			//my->z = lightball_hover_basez - 4 + ((lightball_hover_basez + LIGHTBALL_HOVER_HIGHPEAK - 4 + lightball_hover_basez + LIGHTBALL_HOVER_LOWPEAK - 4) / 2) * sin(lightball_hoverangle * (12.568f / 360.0f)) * 0.1f;
			my->z = lightball_hover_basez + ((lightball_hover_basez + LIGHTBALL_HOVER_HIGHPEAK + lightball_hover_basez + LIGHTBALL_HOVER_LOWPEAK) / 2) * sin(lightball_hoverangle * (12.568f / 360.0f)) * 0.1f;
		}
		lightball_hoverangle += 1;

		//Lightball moving.
		//messagePlayer(0, "*");
		Entity* parent = uidToEntity(my->parent);
		if ( !parent )
		{
			return;
		}
		double distance = sqrt(pow(my->x - parent->x, 2) + pow(my->y - parent->y, 2));
		if ( distance > MAGICLIGHT_BALL_FOLLOW_DISTANCE || my->path)
		{
			lightball_player_lastmove_timer = 0;
			if (lightball_movement_timer > 0)
			{
				lightball_movement_timer--;
			}
			else
			{
				//messagePlayer(0, "****Moving.");
				double tangent = atan2(parent->y - my->y, parent->x - my->x);
				lineTraceTarget(my, my->x, my->y, tangent, 1024, IGNORE_ENTITIES, false, parent);
				if ( !hit.entity || hit.entity == parent )   //Line of sight to caster?
				{
					if (my->path != NULL)
					{
						list_FreeAll(my->path);
						my->path = NULL;
					}
					//double tangent = atan2(parent->y - my->y, parent->x - my->x);
					my->vel_x = cos(tangent) * ((distance - MAGICLIGHT_BALL_FOLLOW_DISTANCE) / MAGICLIGHTBALL_DIVIDE_CONSTANT);
					my->vel_y = sin(tangent) * ((distance - MAGICLIGHT_BALL_FOLLOW_DISTANCE) / MAGICLIGHTBALL_DIVIDE_CONSTANT);
					my->x += (my->vel_x < MAGIC_LIGHTBALL_SPEEDLIMIT) ? my->vel_x : MAGIC_LIGHTBALL_SPEEDLIMIT;
					my->y += (my->vel_y < MAGIC_LIGHTBALL_SPEEDLIMIT) ? my->vel_y : MAGIC_LIGHTBALL_SPEEDLIMIT;
					//} else if (!map.tiles[(int)(OBSTACLELAYER + (my->y / 16) * MAPLAYERS + (my->x / 16) * MAPLAYERS * map.height)]) { //If not in wall..
				}
				else
				{
					//messagePlayer(0, "******Pathfinding.");
					//Caster is not in line of sight. Calculate a move path.
					/*if (my->children->first != NULL) {
						list_RemoveNode(my->children->first);
						my->children->first = NULL;
					}*/
					if (!my->path)
					{
						//messagePlayer(0, "[Light ball] Generating path.");
						list_t* path = generatePath((int)floor(my->x / 16), (int)floor(my->y / 16), (int)floor(parent->x / 16), (int)floor(parent->y / 16), my, parent);
						if ( path != NULL )
						{
							my->path = path;
						}
						else
						{
							//messagePlayer(0, "[Light ball] Failed to generate path (%s line %d).", __FILE__, __LINE__);
						}
					}

					if (my->path)
					{
						double total_distance = 0; //Calculate the total distance to the player to get the right move speed.
						double prevx = my->x;
						double prevy = my->y;
						if (my->path != NULL)
						{
							for (node = my->path->first; node != NULL; node = node->next)
							{
								if (node->element)
								{
									pathnode = (pathnode_t*)node->element;
									//total_distance += sqrt(pow(pathnode->y - prevy, 2) + pow(pathnode->x - prevx, 2) );
									total_distance += sqrt(pow(prevx - pathnode->x, 2) + pow(prevy - pathnode->y, 2) );
									prevx = pathnode->x;
									prevy = pathnode->y;
								}
							}
						}
						else if (my->path)     //If the path has been traversed, reset it.
						{
							list_FreeAll(my->path);
							my->path = NULL;
						}
						total_distance -= MAGICLIGHT_BALL_FOLLOW_DISTANCE;

						if (my->path != NULL)
						{
							if (my->path->first != NULL)
							{
								pathnode = (pathnode_t*)my->path->first->element;
								//double distance = sqrt(pow(pathnode->y * 16 + 8 - my->y, 2) + pow(pathnode->x * 16 + 8 - my->x, 2) );
								//double distance = sqrt(pow((my->y) - ((pathnode->y + 8) * 16), 2) + pow((my->x) - ((pathnode->x + 8) * 16), 2));
								double distance = sqrt(pow(((pathnode->y * 16) + 8) - (my->y), 2) + pow(((pathnode->x * 16) + 8) - (my->x), 2));
								if (distance <= 4)
								{
									list_RemoveNode(pathnode->node); //TODO: Make sure it doesn't get stuck here. Maybe remove the node only if it's the last one?
									if (!my->path->first)
									{
										list_FreeAll(my->path);
										my->path = NULL;
									}
								}
								else
								{
									double target_tangent = atan2((pathnode->y * 16) + 8 - my->y, (pathnode->x * 16) + 8 - my->x);
									if (target_tangent > my->yaw)   //TODO: Do this yaw thing for all movement.
									{
										my->yaw = (target_tangent >= my->yaw + MAGIC_LIGHTBALL_TURNSPEED) ? my->yaw + MAGIC_LIGHTBALL_TURNSPEED : target_tangent;
									}
									else if (target_tangent < my->yaw)
									{
										my->yaw = (target_tangent <= my->yaw - MAGIC_LIGHTBALL_TURNSPEED) ? my->yaw - MAGIC_LIGHTBALL_TURNSPEED : target_tangent;
									}
									my->vel_x = cos(my->yaw) * (total_distance / MAGICLIGHTBALL_DIVIDE_CONSTANT);
									my->vel_y = sin(my->yaw) * (total_distance / MAGICLIGHTBALL_DIVIDE_CONSTANT);
									my->x += (my->vel_x < MAGIC_LIGHTBALL_SPEEDLIMIT) ? my->vel_x : MAGIC_LIGHTBALL_SPEEDLIMIT;
									my->y += (my->vel_y < MAGIC_LIGHTBALL_SPEEDLIMIT) ? my->vel_y : MAGIC_LIGHTBALL_SPEEDLIMIT;
								}
							}
						} //else assertion error, hehehe
					}
					else     //Path failed to generate. Fallback on moving straight to the player.
					{
						//messagePlayer(0, "**************NO PATH WHEN EXPECTED PATH.");
						my->vel_x = cos(tangent) * ((distance) / MAGICLIGHTBALL_DIVIDE_CONSTANT);
						my->vel_y = sin(tangent) * ((distance) / MAGICLIGHTBALL_DIVIDE_CONSTANT);
						my->x += (my->vel_x < MAGIC_LIGHTBALL_SPEEDLIMIT) ? my->vel_x : MAGIC_LIGHTBALL_SPEEDLIMIT;
						my->y += (my->vel_y < MAGIC_LIGHTBALL_SPEEDLIMIT) ? my->vel_y : MAGIC_LIGHTBALL_SPEEDLIMIT;
					}
				} /*else {
					//In a wall. Get out of it.
					double tangent = atan2(parent->y - my->y, parent->x - my->x);
					my->vel_x = cos(tangent) * ((distance) / 100);
					my->vel_y = sin(tangent) * ((distance) / 100);
					my->x += my->vel_x;
					my->y += my->vel_y;
				}*/
			}
		}
		else
		{
			lightball_movement_timer = LIGHTBALL_MOVE_DELAY;
			/*if (lightball_player_lastmove_timer < LIGHTBALL_CIRCLE_TIME) {
				lightball_player_lastmove_timer++;
			} else {
				//TODO: Orbit the player. Maybe.
				my->x = parent->x + (lightball_orbit_length * cos(lightball_orbit_angle));
				my->y = parent->y + (lightball_orbit_length * sin(lightball_orbit_angle));

				lightball_orbit_angle++;
				if (lightball_orbit_angle > 360) {
					lightball_orbit_angle = 0;
				}
			}*/
			if (my->path != NULL)
			{
				list_FreeAll(my->path);
				my->path = NULL;
			}
			if (map.tiles[(int)(OBSTACLELAYER + (my->y / 16) * MAPLAYERS + (my->x / 16) * MAPLAYERS * map.height)])   //If the ball has come to rest in a wall, move its butt.
			{
				double tangent = atan2(parent->y - my->y, parent->x - my->x);
				my->vel_x = cos(tangent) * ((distance) / MAGICLIGHTBALL_DIVIDE_CONSTANT);
				my->vel_y = sin(tangent) * ((distance) / MAGICLIGHTBALL_DIVIDE_CONSTANT);
				my->x += my->vel_x;
				my->y += my->vel_y;
			}
		}

		//Light up the area.
		my->light = lightSphereShadow(my->x / 16, my->y / 16, 8, 192);
		lightball_flicker++;

		if (lightball_flicker > 5)
		{
			lightball_lighting = (lightball_lighting == 1) + 1;

			if (lightball_lighting == 1)
			{
				my->removeLightField();
				my->light = lightSphereShadow(my->x / 16, my->y / 16, 8, 192);
			}
			else
			{
				my->removeLightField();
				my->light = lightSphereShadow(my->x / 16, my->y / 16, 8, 174);
			}
			lightball_flicker = 0;
		}

		lightball_timer--;
	}
	else
	{
		//Init the lightball. That is, shoot out from the player.

		//Move out from the player.
		my->vel_x = cos(my->yaw) * 4;
		my->vel_y = sin(my->yaw) * 4;
		double dist = clipMove(&my->x, &my->y, my->vel_x, my->vel_y, my);

		unsigned int distance = sqrt(pow(my->x - lightball_player_startx, 2) + pow(my->y - lightball_player_starty, 2));
		if (distance > MAGICLIGHT_BALL_FOLLOW_DISTANCE * 2 || dist != sqrt(my->vel_x * my->vel_x + my->vel_y * my->vel_y))
		{
			magic_init = 1;
			my->sprite = 174; //Go from black ball to white ball.
			lightball_lighting = 1;
			lightball_movement_timer = 0; //Start off at 0 so that it moves towards the player as soon as it's created (since it's created farther away from the player).
		}
	}
}

void actMagicMissile(Entity* my)   //TODO: Verify this function.
{
	if (!my || !my->children.first || !my->children.first->element)
	{
		return;
	}
	spell_t* spell = (spell_t*)my->children.first->element;
	if (!spell)
	{
		return;
	}
	//node_t *node = NULL;
	spellElement_t* element = NULL;
	node_t* node = NULL;
	int i = 0;
	int c = 0;
	Entity* entity = NULL;
	double tangent;

	Entity* parent = uidToEntity(my->parent);

	if (magic_init)
	{
		my->removeLightField();

		if (clientnum == 0 || multiplayer == SERVER)
		{
			//Handle the missile's life.
			MAGIC_LIFE++;

			if (MAGIC_LIFE >= MAGIC_MAXLIFE)
			{
				list_RemoveNode(my->mynode);
				return;
			}

			node = spell->elements.first;
			//element = (spellElement_t *) spell->elements->first->element;
			element = (spellElement_t*)node->element;
			Sint32 entityHealth = 0;
			double dist = 0.f;
			bool hitFromAbove = false;
			if ( parent && parent->behavior == &actMagicTrapCeiling )
			{
				// moving vertically.
				my->z += my->vel_z;
				hitFromAbove = my->magicFallingCollision();
				if ( !hitFromAbove )
				{
					// nothing hit yet, let's keep trying...
				}
			}
			else
			{
				dist = clipMove(&my->x, &my->y, my->vel_x, my->vel_y, my);
			}

			if ( hitFromAbove || dist != sqrt(my->vel_x * my->vel_x + my->vel_y * my->vel_y) )
			{
				node = element->elements.first;
				//element = (spellElement_t *) element->elements->first->element;
				element = (spellElement_t*)node->element;
				//if (hit.entity != NULL) {
				Stat* hitstats = NULL;
				int player = -1;
				if (hit.entity)
				{
					hitstats = hit.entity->getStats();
					if ( hit.entity->behavior == &actPlayer )
					{
						player = hit.entity->skill[2];
						Uint32 color = SDL_MapRGB(mainsurface->format, 255, 0, 0);
						messagePlayerColor(player, color, language[376]);
						if ( hitstats )
						{
							entityHealth = hitstats->HP;
						}
					}
					if ( parent && hitstats )
					{
						if ( parent->behavior == &actPlayer )
						{
							Uint32 color = SDL_MapRGB(mainsurface->format, 0, 255, 0);
							if ( strcmp(hitstats->name, "") )
							{
								messagePlayerColor(parent->skill[2], color, language[377], hitstats->name);
							}
							else
							{
								if ( hitstats->type < KOBOLD ) //Original monster count
								{
									messagePlayerColor(parent->skill[2], color, language[378], language[90 + hitstats->type]);
								}
								else if ( hitstats->type >= KOBOLD ) //New monsters
								{
									messagePlayerColor(parent->skill[2], color, language[378], language[2000 + (hitstats->type - KOBOLD)]);
								}
							}
						}
					}
				}

				if (hit.entity)
				{
					// alert the hit entity if it was a monster
					if ( hit.entity->behavior == &actMonster && parent != nullptr )
					{
						if ( hit.entity->monsterState != MONSTER_STATE_ATTACK && (hitstats->type < LICH || hitstats->type >= SHOPKEEPER) )
						{
							/*hit.entity->monsterState = MONSTER_STATE_PATH;
							hit.entity->monsterTarget = parent->getUID();
							hit.entity->monsterTargetX = parent->x;
							hit.entity->monsterTargetY = parent->y;*/

							hit.entity->monsterAcquireAttackTarget(*parent, MONSTER_STATE_PATH);
						}

						// alert other monsters too
						Entity* ohitentity = hit.entity;
						for ( node = map.entities->first; node != nullptr; node = node->next )
						{
							entity = (Entity*)node->element;
							if ( entity->behavior == &actMonster && entity != ohitentity )
							{
								Stat* buddystats = entity->getStats();
								if ( buddystats != nullptr )
								{
									if ( hit.entity && hit.entity->checkFriend(entity) ) //TODO: hit.entity->checkFriend() without first checking if it's NULL crashes because hit.entity turns to NULL somewhere along the line. It looks like ohitentity preserves that value though, so....uh...ya, I don't know.
									{
										if ( entity->monsterState == MONSTER_STATE_WAIT )
										{
											tangent = atan2( entity->y - ohitentity->y, entity->x - ohitentity->x );
											lineTrace(ohitentity, ohitentity->x, ohitentity->y, tangent, 1024, 0, false);
											if ( hit.entity == entity )
											{
												/*entity->monsterState = MONSTER_STATE_PATH;
												entity->monsterTarget = parent->getUID();
												entity->monsterTargetX = parent->x;
												entity->monsterTargetY = parent->y;*/

												entity->monsterAcquireAttackTarget(*parent, MONSTER_STATE_PATH);
											}
										}
									}
								}
							}
						}
						hit.entity = ohitentity;
					}
				}

				// check for magic reflection...
				int reflection = 0;
				if ( hitstats )
				{
					if ( !strcmp(map.name, "Hell Boss") && hit.entity->behavior == &actPlayer )
					{
						bool founddevil = false;
						node_t* tempNode;
						for ( tempNode = map.entities->first; tempNode != NULL; tempNode = tempNode->next )
						{
							Entity* tempEntity = (Entity*)tempNode->element;
							if ( tempEntity->behavior == &actMonster )
							{
								Stat* stats = tempEntity->getStats();
								if ( stats )
								{
									if ( stats->type == DEVIL )
									{
										founddevil = true;
										break;
									}
								}
							}
						}
						if ( !founddevil )
						{
							reflection = 3;
						}
					}
					if ( !reflection )
					{
						reflection = hit.entity->getReflection();
					}
				}
				if ( reflection )
				{
					spell_t* spellIsReflectingMagic = hit.entity->getActiveMagicEffect(SPELL_REFLECT_MAGIC);
					playSoundEntity(hit.entity, 166, 128);
					if (hit.entity)
					{
						if ( hit.entity->behavior == &actPlayer )
						{
							if ( !spellIsReflectingMagic )
							{
								messagePlayer(player, language[379]);
							}
							else
							{
								messagePlayer(player, language[2475]);
							}
						}
					}
					if ( parent )
					{
						if ( parent->behavior == &actPlayer )
						{
							messagePlayer(parent->skill[2], language[379]);
						}
					}
					if ( hit.side == HORIZONTAL )
					{
						my->vel_x *= -1;
					}
					else if ( hit.side == VERTICAL )
					{
						my->vel_y *= -1;
					}
					if (hit.entity)
					{
						if ( parent && parent->behavior == &actMagicTrapCeiling )
						{
							// this missile came from the ceiling, let's redirect it..
							my->x = hit.entity->x + cos(hit.entity->yaw);
							my->y = hit.entity->y + sin(hit.entity->yaw);
							my->yaw = hit.entity->yaw;
							my->z = -1;
							my->vel_x = 4 * cos(hit.entity->yaw);
							my->vel_y = 4 * sin(hit.entity->yaw);
							my->vel_z = 0;
							my->pitch = 0;
						}
						my->parent = hit.entity->getUID();
					}
					
					// Test for Friendly Fire, if Friendly Fire is OFF, delete the missile
					bool bShouldEquipmentDegrade = false;
					if ( (svFlags & SV_FLAG_FRIENDLYFIRE) )
					{
						// Friendly Fire is ON, equipment should always degrade, as hit will register
						bShouldEquipmentDegrade = true;
					}
					else
					{
						// Friendly Fire is OFF, is the target an enemy?
						if ( parent != nullptr && (parent->checkFriend(hit.entity)) == false )
						{
							// Target is an enemy, equipment should degrade
							bShouldEquipmentDegrade = true;
						}
					}

					if ( bShouldEquipmentDegrade )
					{
						// Reflection of 3 does not degrade equipment
						if ( rand() % 2 == 0 && hitstats && reflection < 3 )
						{
							// set armornum to the relevant equipment slot to send to clients
							int armornum = 5 + reflection;
							if ( player == clientnum || player < 0 )
							{
								if ( reflection == 1 )
								{
									if ( hitstats->cloak->count > 1 )
									{
										newItem(hitstats->cloak->type, hitstats->cloak->status, hitstats->cloak->beatitude, hitstats->cloak->count - 1, hitstats->cloak->appearance, hitstats->cloak->identified, &hitstats->inventory);
									}
								}
								else if ( reflection == 2 )
								{
									if ( hitstats->amulet->count > 1 )
									{
										newItem(hitstats->amulet->type, hitstats->amulet->status, hitstats->amulet->beatitude, hitstats->amulet->count - 1, hitstats->amulet->appearance, hitstats->amulet->identified, &hitstats->inventory);
									}
								}
								else if ( reflection == -1 )
								{
									if ( hitstats->shield->count > 1 )
									{
										newItem(hitstats->shield->type, hitstats->shield->status, hitstats->shield->beatitude, hitstats->shield->count - 1, hitstats->shield->appearance, hitstats->shield->identified, &hitstats->inventory);
									}
								}
							}
							if ( reflection == 1 )
							{
								hitstats->cloak->count = 1;
								hitstats->cloak->status = static_cast<Status>(hitstats->cloak->status - 1);
								if ( hitstats->cloak->status != BROKEN )
								{
									messagePlayer(player, language[380]);
								}
								else
								{
									messagePlayer(player, language[381]);
									playSoundEntity(hit.entity, 76, 64);
								}
							}
							else if ( reflection == 2 )
							{
								hitstats->amulet->count = 1;
								hitstats->amulet->status = static_cast<Status>(hitstats->amulet->status - 1);
								if ( hitstats->amulet->status != BROKEN )
								{
									messagePlayer(player, language[382]);
								}
								else
								{
									messagePlayer(player, language[383]);
									playSoundEntity(hit.entity, 76, 64);
								}
							}
							else if ( reflection == -1 )
							{
								hitstats->shield->count = 1;
								hitstats->shield->status = static_cast<Status>(hitstats->shield->status - 1);
								if ( hitstats->shield->status != BROKEN )
								{
									messagePlayer(player, language[384]);
								}
								else
								{
									messagePlayer(player, language[385]);
									playSoundEntity(hit.entity, 76, 64);
								}
							}
							if ( player > 0 && multiplayer == SERVER )
							{
								strcpy((char*)net_packet->data, "ARMR");
								net_packet->data[4] = armornum;
								if ( reflection == 1 )
								{
									net_packet->data[5] = hitstats->cloak->status;
								}
								else if ( reflection == 2 )
								{
									net_packet->data[5] = hitstats->amulet->status;
								}
								else
								{
									net_packet->data[5] = hitstats->shield->status;
								}
								net_packet->address.host = net_clients[player - 1].host;
								net_packet->address.port = net_clients[player - 1].port;
								net_packet->len = 6;
								sendPacketSafe(net_sock, -1, net_packet, player - 1);
							}
						}
					}
					
					if ( spellIsReflectingMagic )
					{
						int spellCost = getCostOfSpell(spell);
						bool unsustain = false;
						if ( spellCost >= hit.entity->getMP() ) //Unsustain the spell if expended all mana.
						{
							unsustain = true;
						}

						hit.entity->drainMP(spellCost);
						spawnMagicEffectParticles(hit.entity->x, hit.entity->y, hit.entity->z / 2, 174);
						playSoundEntity(hit.entity, 166, 128); //TODO: Custom sound effect?

						if ( unsustain )
						{
							spellIsReflectingMagic->sustain = false;
							if ( hitstats )
							{
								hit.entity->setEffect(EFF_MAGICREFLECT, false, 0, true);
								messagePlayer(player, language[2476]);
							}
						}
					}
					return;
				}

				// check for magic resistance...
				// resistance stacks diminishingly
				//TODO: EFFECTS[EFF_MAGICRESIST]
				int resistance = 0;
				if ( hitstats )
				{
					if ( hitstats->shield )
					{
						if ( hitstats->shield->type == STEEL_SHIELD_RESISTANCE )
						{
							if ( hitstats->defending )
							{
								resistance += 2;
							}
							else
							{
								resistance += 1;
							}
						}
					}
					if ( hitstats->ring )
					{
						if ( hitstats->ring->type == RING_MAGICRESISTANCE )
						{
							resistance += 1;
						}
					}
					if ( hitstats->gloves )
					{
						if ( hitstats->gloves->type == ARTIFACT_GLOVES )
						{
							resistance += 1;
						}
					}
					if ( hitstats->EFFECTS[EFF_MAGICRESIST] )
					{
						resistance += 1;
					}
				}
				if ( resistance > 0 )
				{
					if ( parent )
					{
						if ( parent->behavior == &actPlayer )
						{
							messagePlayer(parent->skill[2], language[386]);
						}
					}
				}

				if (!strcmp(element->name, spellElement_force.name))
				{
					if (hit.entity)
					{
						if (hit.entity->behavior == &actMonster || hit.entity->behavior == &actPlayer)
						{
							Entity* parent = uidToEntity(my->parent);
							if ( !(svFlags & SV_FLAG_FRIENDLYFIRE) )
							{
								// test for friendly fire
								if ( parent && parent->checkFriend(hit.entity) )
								{
									list_RemoveNode(my->mynode);
									return;
								}
							}
							playSoundEntity(hit.entity, 28, 128);
							int damage = element->damage;
							//damage += ((element->mana - element->base_mana) / static_cast<double>(element->overload_multiplier)) * element->damage;
							damage *= damagetables[hitstats->type][5];
							damage /= (1 + (int)resistance);
							hit.entity->modHP(-damage);
							for (i = 0; i < damage; i += 2)   //Spawn a gib for every two points of damage.
							{
								spawnGib(hit.entity);
							}

							if (parent)
							{
								parent->killedByMonsterObituary(hit.entity);
							}

							// update enemy bar for attacker
							if ( !strcmp(hitstats->name, "") )
							{
								if ( hitstats->type < KOBOLD ) //Original monster count
								{
									updateEnemyBar(parent, hit.entity, language[90 + hitstats->type], hitstats->HP, hitstats->MAXHP);
								}
								else if ( hitstats->type >= KOBOLD ) //New monsters
								{
									updateEnemyBar(parent, hit.entity, language[2000 + (hitstats->type - KOBOLD)], hitstats->HP, hitstats->MAXHP);
								}
							}
							else
							{
								updateEnemyBar(parent, hit.entity, hitstats->name, hitstats->HP, hitstats->MAXHP);
							}

							if ( hitstats->HP <= 0 && parent)
							{
								parent->awardXP( hit.entity, true, true );
							}
						}
						else if (hit.entity->behavior == &actDoor)
						{
							int damage = element->damage;
							damage /= (1 + (int)resistance);

							hit.entity->doorHandleDamageMagic(damage, *my, parent);

							my->removeLightField();
							list_RemoveNode(my->mynode);
							return;
							/*} else if (hit.entity->behavior == &actChest) { //TODO: Get right skill values and language file entries.
								int damage = element->damage;
								damage /= (1+(int)resistance);

								hit.entity->skill[3] -= damage; //Decrease chest health.
								if( hit.entity->skill[3] < 0 )
									if( parent )
										if( parent->behavior == &actPlayer )
											messagePlayer(parent->skill[2],language[387]);
								playSoundEntity(hit.entity, 28, 128);
								if( !hit.entity->skill[0] )
									hit.entity->skill[6] = (my->x > hit.entity->x);
								else
									hit.entity->skill[6] = (my->y < hit.entity->y);
								my->removeLightField();
								updateEnemyBar(parent,hit.entity,language[674],hit.entity->skill[3],hit.entity->skill[9]);
								list_RemoveNode(my->mynode);
								return;*/
						}
						else if (hit.entity->behavior == &actFurniture )
						{
							int damage = element->damage;
							damage /= (1 + (int)resistance);
							hit.entity->furnitureHealth -= damage;
							if ( hit.entity->furnitureHealth < 0 )
							{
								if ( parent )
								{
									if ( parent->behavior == &actPlayer )
									{
										switch ( hit.entity->furnitureType )
										{
											case FURNITURE_CHAIR:
												messagePlayer(parent->skill[2], language[388]);
												updateEnemyBar(parent, hit.entity, language[677], hit.entity->furnitureHealth, hit.entity->furnitureMaxHealth);
												break;
											case FURNITURE_TABLE:
												messagePlayer(parent->skill[2], language[389]);
												updateEnemyBar(parent, hit.entity, language[676], hit.entity->furnitureHealth, hit.entity->furnitureMaxHealth);
												break;
											case FURNITURE_BED:
												messagePlayer(parent->skill[2], language[2508], language[2505]);
												updateEnemyBar(parent, hit.entity, language[2505], hit.entity->furnitureHealth, hit.entity->furnitureMaxHealth);
												break;
											case FURNITURE_BUNKBED:
												messagePlayer(parent->skill[2], language[2508], language[2506]);
												updateEnemyBar(parent, hit.entity, language[2506], hit.entity->furnitureHealth, hit.entity->furnitureMaxHealth);
												break;
											case FURNITURE_PODIUM:
												messagePlayer(parent->skill[2], language[2508], language[2507]);
												updateEnemyBar(parent, hit.entity, language[2507], hit.entity->furnitureHealth, hit.entity->furnitureMaxHealth);
												break;
											default:
												break;
										}
									}
								}
							}
							playSoundEntity(hit.entity, 28, 128);
						}
					}
				}
				else if (!strcmp(element->name, spellElement_magicmissile.name))
				{
					spawnExplosion(my->x, my->y, my->z);
					if (hit.entity)
					{
						if (hit.entity->behavior == &actMonster || hit.entity->behavior == &actPlayer)
						{
							Entity* parent = uidToEntity(my->parent);
							if ( !(svFlags & SV_FLAG_FRIENDLYFIRE) )
							{
								// test for friendly fire
								if ( parent &&  parent->checkFriend(hit.entity) )
								{
									my->removeLightField();
									list_RemoveNode(my->mynode);
									return;
								}
							}
							playSoundEntity(hit.entity, 28, 128);
							int damage = element->damage;
							//damage += ((element->mana - element->base_mana) / static_cast<double>(element->overload_multiplier)) * element->damage;
							damage *= damagetables[hitstats->type][5];
							damage /= (1 + (int)resistance);
							hit.entity->modHP(-damage);
							for (i = 0; i < damage; i += 2)   //Spawn a gib for every two points of damage.
							{
								spawnGib(hit.entity);
							}

							// write the obituary
							if (parent)
							{
								parent->killedByMonsterObituary(hit.entity);
							}

							// update enemy bar for attacker
							if ( !strcmp(hitstats->name, "") )
							{
								if ( hitstats->type < KOBOLD ) //Original monster count
								{
									updateEnemyBar(parent, hit.entity, language[90 + hitstats->type], hitstats->HP, hitstats->MAXHP);
								}
								else if ( hitstats->type >= KOBOLD ) //New monsters
								{
									updateEnemyBar(parent, hit.entity, language[2000 + (hitstats->type - KOBOLD)], hitstats->HP, hitstats->MAXHP);
								}
							}
							else
							{
								updateEnemyBar(parent, hit.entity, hitstats->name, hitstats->HP, hitstats->MAXHP);
							}

							if ( hitstats->HP <= 0 && parent)
							{
								parent->awardXP( hit.entity, true, true );
							}
						}
						else if (hit.entity->behavior == &actDoor)
						{
							int damage = element->damage;
							//damage += ((element->mana - element->base_mana) / static_cast<double>(element->overload_multiplier)) * element->damage;
							damage /= (1 + (int)resistance);

							hit.entity->doorHandleDamageMagic(damage, *my, parent);

							my->removeLightField();
							list_RemoveNode(my->mynode);
							return;
							/*} else if (hit.entity->behavior == &actChest) { //TODO: Get right skill values and language file entries.
								int damage = element->damage;
								//damage += ((element->mana - element->base_mana) / static_cast<double>(element->overload_multiplier)) * element->damage;
								damage /= (1+(int)resistance);

								hit.entity->skill[3] -= damage; //Decrease chest health.
								if( hit.entity->skill[3] < 0 )
									if( parent )
										if( parent->behavior == &actPlayer )
											messagePlayer(parent->skill[2],language[387]);
								playSoundEntity(hit.entity, 28, 128);
								if( !hit.entity->skill[0] )
									hit.entity->skill[6] = (my->x > hit.entity->x);
								else
									hit.entity->skill[6] = (my->y < hit.entity->y);

								my->removeLightField();
								updateEnemyBar(parent,hit.entity,language[674],hit.entity->skill[3],hit.entity->skill[9]);
								list_RemoveNode(my->mynode);
								return;*/
						}
						else if (hit.entity->behavior == &actFurniture )
						{
							int damage = element->damage;
							damage /= (1 + (int)resistance);
							hit.entity->furnitureHealth -= damage;
							if ( hit.entity->furnitureHealth < 0 )
							{
								if ( parent )
								{
									if ( parent->behavior == &actPlayer )
									{
										switch ( hit.entity->furnitureType )
										{
											case FURNITURE_CHAIR:
												messagePlayer(parent->skill[2], language[388]);
												updateEnemyBar(parent, hit.entity, language[677], hit.entity->furnitureHealth, hit.entity->furnitureMaxHealth);
												break;
											case FURNITURE_TABLE:
												messagePlayer(parent->skill[2], language[389]);
												updateEnemyBar(parent, hit.entity, language[676], hit.entity->furnitureHealth, hit.entity->furnitureMaxHealth);
												break;
											case FURNITURE_BED:
												messagePlayer(parent->skill[2], language[2508], language[2505]);
												updateEnemyBar(parent, hit.entity, language[2505], hit.entity->furnitureHealth, hit.entity->furnitureMaxHealth);
												break;
											case FURNITURE_BUNKBED:
												messagePlayer(parent->skill[2], language[2508], language[2506]);
												updateEnemyBar(parent, hit.entity, language[2506], hit.entity->furnitureHealth, hit.entity->furnitureMaxHealth);
												break;
											case FURNITURE_PODIUM:
												messagePlayer(parent->skill[2], language[2508], language[2507]);
												updateEnemyBar(parent, hit.entity, language[2507], hit.entity->furnitureHealth, hit.entity->furnitureMaxHealth);
												break;
											default:
												break;
										}
									}
								}
							}
							playSoundEntity(hit.entity, 28, 128);
							my->removeLightField();
							list_RemoveNode(my->mynode);
							return;
						}
					}
				}
				else if (!strcmp(element->name, spellElement_fire.name))
				{
					spawnExplosion(my->x, my->y, my->z);
					if (hit.entity)
					{
						if ( hit.entity->flags[BURNABLE] )
							if ( !hit.entity->flags[BURNING] )
							{
								hit.entity->flags[BURNING] = true;
								serverUpdateEntityFlag(hit.entity, BURNING);
							}
						if (hit.entity->behavior == &actMonster || hit.entity->behavior == &actPlayer)
						{
							if ( !(svFlags & SV_FLAG_FRIENDLYFIRE) )
							{
								// test for friendly fire
								if ( parent && parent->checkFriend(hit.entity) )
								{
									my->removeLightField();
									list_RemoveNode(my->mynode);
									return;
								}
							}
							//playSoundEntity(my, 153, 64);
							playSoundEntity(hit.entity, 28, 128);
							//TODO: Apply fire resistances/weaknesses.
							int damage = element->damage;
							//damage += ((element->mana - element->base_mana) / static_cast<double>(element->overload_multiplier)) * element->damage;
							damage *= damagetables[hitstats->type][5];
							damage /= (1 + (int)resistance);
							hit.entity->modHP(-damage);
							//for (i = 0; i < damage; i += 2) { //Spawn a gib for every two points of damage.
							spawnGib(hit.entity);
							//}

							// write the obituary
							hit.entity->setObituary(language[1501]);
							if ( hitstats )
							{
								hitstats->poisonKiller = my->parent;
							}

							// update enemy bar for attacker
							if ( !strcmp(hitstats->name, "") )
							{
								if ( hitstats->type < KOBOLD ) //Original monster count
								{
									updateEnemyBar(parent, hit.entity, language[90 + hitstats->type], hitstats->HP, hitstats->MAXHP);
								}
								else if ( hitstats->type >= KOBOLD ) //New monsters
								{
									updateEnemyBar(parent, hit.entity, language[2000 + (hitstats->type - KOBOLD)], hitstats->HP, hitstats->MAXHP);
								}
							}
							else
							{
								updateEnemyBar(parent, hit.entity, hitstats->name, hitstats->HP, hitstats->MAXHP);
							}
							if ( hitstats->HP <= 0 && parent)
							{
								parent->awardXP( hit.entity, true, true );
							}
						}
						else if (hit.entity->behavior == &actDoor)
						{
							int damage = element->damage;
							damage /= (1 + (int)resistance);

							hit.entity->doorHandleDamageMagic(damage, *my, parent);

							my->removeLightField();
							list_RemoveNode(my->mynode);
							return;
							/*} else if (hit.entity->behavior == &actChest) { //TODO: Get right skill values and language file entries.
								int damage = element->damage;
								damage /= (1+(int)resistance);

								hit.entity->skill[3] -= damage; //Decrease chest health.
								if( hit.entity->skill[3] < 0 )
									if( parent )
										if( parent->behavior == &actPlayer )
											messagePlayer(parent->skill[2],language[387]);
								playSoundEntity(hit.entity, 28, 128);
								if( !hit.entity->skill[0] )
									hit.entity->skill[6] = (my->x > hit.entity->x);
								else
									hit.entity->skill[6] = (my->y < hit.entity->y);
								my->removeLightField();
								updateEnemyBar(parent,hit.entity,language[674],hit.entity->skill[3],hit.entity->skill[9]);
								list_RemoveNode(my->mynode);
								return;*/
						}
						else if (hit.entity->behavior == &actFurniture )
						{
							int damage = element->damage;
							damage /= (1 + (int)resistance);
							hit.entity->furnitureHealth -= damage;
							if ( hit.entity->furnitureHealth < 0 )
							{
								if ( parent )
								{
									if ( parent->behavior == &actPlayer )
									{
										switch ( hit.entity->furnitureType )
										{
											case FURNITURE_CHAIR:
												messagePlayer(parent->skill[2], language[388]);
												updateEnemyBar(parent, hit.entity, language[677], hit.entity->furnitureHealth, hit.entity->furnitureMaxHealth);
												break;
											case FURNITURE_TABLE:
												messagePlayer(parent->skill[2], language[389]);
												updateEnemyBar(parent, hit.entity, language[676], hit.entity->furnitureHealth, hit.entity->furnitureMaxHealth);
												break;
											case FURNITURE_BED:
												messagePlayer(parent->skill[2], language[2508], language[2505]);
												updateEnemyBar(parent, hit.entity, language[2505], hit.entity->furnitureHealth, hit.entity->furnitureMaxHealth);
												break;
											case FURNITURE_BUNKBED:
												messagePlayer(parent->skill[2], language[2508], language[2506]);
												updateEnemyBar(parent, hit.entity, language[2506], hit.entity->furnitureHealth, hit.entity->furnitureMaxHealth);
												break;
											case FURNITURE_PODIUM:
												messagePlayer(parent->skill[2], language[2508], language[2507]);
												updateEnemyBar(parent, hit.entity, language[2507], hit.entity->furnitureHealth, hit.entity->furnitureMaxHealth);
												break;
											default:
												break;
										}
									}
								}
							}
							playSoundEntity(hit.entity, 28, 128);
							my->removeLightField();
							list_RemoveNode(my->mynode);
							return;
						}
					}
				}
				else if (!strcmp(element->name, spellElement_confuse.name))
				{
					if (hit.entity)
					{
						if (hit.entity->behavior == &actMonster || hit.entity->behavior == &actPlayer)
						{
							if ( !(svFlags & SV_FLAG_FRIENDLYFIRE) )
							{
								// test for friendly fire
								if ( parent && parent->checkFriend(hit.entity) )
								{
									if ( my->light != NULL )
									{
										list_RemoveNode(my->light->node);
										my->light = NULL;
									}
									list_RemoveNode(my->mynode);
									return;
								}
							}
							playSoundEntity(hit.entity, 174, 64);
							hitstats->EFFECTS[EFF_CONFUSED] = true;
							hitstats->EFFECTS_TIMERS[EFF_CONFUSED] = (element->duration * (((element->mana) / static_cast<double>(element->base_mana)) * element->overload_multiplier));
							hitstats->EFFECTS_TIMERS[EFF_CONFUSED] /= (1 + (int)resistance);

							// If the Entity hit is a Player, update their status to be Slowed
							if ( hit.entity->behavior == &actPlayer )
							{
								serverUpdateEffects(hit.entity->skill[2]);
							}

							hit.entity->skill[1] = 0; //Remove the monster's target.
							if ( parent )
							{
								Uint32 color = SDL_MapRGB(mainsurface->format, 0, 255, 0);
								if ( parent->behavior == &actPlayer )
								{
									if ( strcmp(hitstats->name, "") )
									{
										messagePlayerColor(parent->skill[2], color, language[390], hitstats->name);
									}
									else
									{
										if ( hitstats->type < KOBOLD ) //Original monster count
										{
											messagePlayerColor(parent->skill[2], color, language[391], language[90 + hitstats->type]);
										}
										else if ( hitstats->type >= KOBOLD ) //New monsters
										{
											messagePlayerColor(parent->skill[2], color, language[391], language[2000 + (hitstats->type - KOBOLD)]);
										}
									}
								}
							}
							Uint32 color = SDL_MapRGB(mainsurface->format, 255, 0, 0);
							if ( player >= 0 )
							{
								messagePlayerColor(player, color, language[392]);
							}
							spawnMagicEffectParticles(hit.entity->x, hit.entity->y, hit.entity->z, my->sprite);
						}
					}
				}
				else if (!strcmp(element->name, spellElement_cold.name))
				{
					playSoundEntity(my, 197, 128);
					if (hit.entity)
					{
						if (hit.entity->behavior == &actMonster || hit.entity->behavior == &actPlayer)
						{
							if ( !(svFlags & SV_FLAG_FRIENDLYFIRE) )
							{
								// test for friendly fire
								if ( parent && parent->checkFriend(hit.entity) )
								{
									my->removeLightField();
									list_RemoveNode(my->mynode);
									return;
								}
							}
							playSoundEntity(hit.entity, 172, 64);
							hitstats->EFFECTS[EFF_SLOW] = true;
							hitstats->EFFECTS_TIMERS[EFF_SLOW] = (element->duration * (((element->mana) / static_cast<double>(element->base_mana)) * element->overload_multiplier));
							hitstats->EFFECTS_TIMERS[EFF_SLOW] /= (1 + (int)resistance);

							// If the Entity hit is a Player, update their status to be Slowed
							if ( hit.entity->behavior == &actPlayer )
							{
								serverUpdateEffects(hit.entity->skill[2]);
							}

							int damage = element->damage;
							//damage += ((element->mana - element->base_mana) / static_cast<double>(element->overload_multiplier)) * element->damage;
							damage *= damagetables[hitstats->type][5];
							damage /= (1 + (int)resistance);
							hit.entity->modHP(-damage);
							spawnGib(hit.entity);

							// write the obituary
							hit.entity->setObituary(language[1502]);

							// update enemy bar for attacker
							if ( !strcmp(hitstats->name, "") )
							{
								if ( hitstats->type < KOBOLD ) //Original monster count
								{
									updateEnemyBar(parent, hit.entity, language[90 + hitstats->type], hitstats->HP, hitstats->MAXHP);
								}
								else if ( hitstats->type >= KOBOLD ) //New monsters
								{
									updateEnemyBar(parent, hit.entity, language[2000 + (hitstats->type - KOBOLD)], hitstats->HP, hitstats->MAXHP);
								}
							}
							else
							{
								updateEnemyBar(parent, hit.entity, hitstats->name, hitstats->HP, hitstats->MAXHP);
							}
							if ( parent )
							{
								Uint32 color = SDL_MapRGB(mainsurface->format, 0, 255, 0);
								if ( parent->behavior == &actPlayer )
								{
									if ( strcmp(hitstats->name, "") )
									{
										messagePlayerColor(parent->skill[2], color, language[393], hitstats->name);
									}
									else
									{
										if ( hitstats->type < KOBOLD ) //Original monster count
										{
											messagePlayerColor(parent->skill[2], color, language[394], language[90 + hitstats->type]);
										}
										else if ( hitstats->type >= KOBOLD ) //New monsters
										{
											messagePlayerColor(parent->skill[2], color, language[394], language[2000 + (hitstats->type - KOBOLD)]);
										}
									}
								}
							}
							Uint32 color = SDL_MapRGB(mainsurface->format, 255, 0, 0);
							if ( player >= 0 )
							{
								messagePlayerColor(player, color, language[395]);
							}
						}
					}
				}
				else if (!strcmp(element->name, spellElement_slow.name))
				{
					if (hit.entity)
					{
						if (hit.entity->behavior == &actMonster || hit.entity->behavior == &actPlayer)
						{
							if ( !(svFlags & SV_FLAG_FRIENDLYFIRE) )
							{
								// test for friendly fire
								if ( parent && parent->checkFriend(hit.entity) )
								{
									my->removeLightField();
									list_RemoveNode(my->mynode);
									return;
								}
							}
							playSoundEntity(hit.entity, 172, 64); //TODO: Slow spell sound.
							hitstats->EFFECTS[EFF_SLOW] = true;
							hitstats->EFFECTS_TIMERS[EFF_SLOW] = (element->duration * (((element->mana) / static_cast<double>(element->base_mana)) * element->overload_multiplier));
							hitstats->EFFECTS_TIMERS[EFF_SLOW] /= (1 + (int)resistance);

							// If the Entity hit is a Player, update their status to be Slowed
							if ( hit.entity->behavior == &actPlayer )
							{
								serverUpdateEffects(hit.entity->skill[2]);
							}

							// update enemy bar for attacker
							if ( parent )
							{
								Uint32 color = SDL_MapRGB(mainsurface->format, 0, 255, 0);
								if ( parent->behavior == &actPlayer )
								{
									if ( strcmp(hitstats->name, "") )
									{
										messagePlayerColor(parent->skill[2], color, language[393], hitstats->name);
									}
									else
									{
										if ( hitstats->type < KOBOLD ) //Original monster count
										{
											messagePlayerColor(parent->skill[2], color, language[394], language[90 + hitstats->type]);
										}
										else if ( hitstats->type >= KOBOLD ) //New monsters
										{
											messagePlayerColor(parent->skill[2], color, language[394], language[2000 + (hitstats->type - KOBOLD)]);
										}
									}
								}
							}
							Uint32 color = SDL_MapRGB(mainsurface->format, 255, 0, 0);
							if ( player >= 0 )
							{
								messagePlayerColor(player, color, language[395]);
							}
							spawnMagicEffectParticles(hit.entity->x, hit.entity->y, hit.entity->z, my->sprite);
						}
					}
				}
				else if (!strcmp(element->name, spellElement_sleep.name))
				{
					if (hit.entity)
					{
						if (hit.entity->behavior == &actMonster || hit.entity->behavior == &actPlayer)
						{
							if ( !(svFlags & SV_FLAG_FRIENDLYFIRE) )
							{
								// test for friendly fire
								if ( parent && parent->checkFriend(hit.entity) )
								{
									my->removeLightField();
									list_RemoveNode(my->mynode);
									return;
								}
							}
							playSoundEntity(hit.entity, 174, 64);
							hitstats->EFFECTS[EFF_ASLEEP] = true;
							if ( parent && parent->behavior == &actMagicTrapCeiling )
							{
								hitstats->EFFECTS_TIMERS[EFF_ASLEEP] = 200 + rand() % 150; // 4 seconds + 0 to 3 seconds.
							}
							else
							{
								hitstats->EFFECTS_TIMERS[EFF_ASLEEP] = 600 + rand() % 300; // 12 seconds + 0 to 6 seconds.
							}
							hitstats->EFFECTS_TIMERS[EFF_ASLEEP] /= (1 + (int)resistance);
							hitstats->OLDHP = hitstats->HP;
							if ( hit.entity->behavior == &actPlayer )
							{
								serverUpdateEffects(hit.entity->skill[2]);
								Uint32 color = SDL_MapRGB(mainsurface->format, 255, 0, 0);
								messagePlayerColor(hit.entity->skill[2], color, language[396]);
							}
							if ( parent )
							{
								Uint32 color = SDL_MapRGB(mainsurface->format, 0, 255, 0);
								if ( parent->behavior == &actPlayer )
								{
									if ( strcmp(hitstats->name, "") )
									{
										messagePlayerColor(parent->skill[2], color, language[397], hitstats->name);
									}
									else
									{
										if ( hitstats->type < KOBOLD ) //Original monster count
										{
											messagePlayerColor(parent->skill[2], color, language[398], language[90 + hitstats->type]);
										}
										else if ( hitstats->type >= KOBOLD ) //New monsters
										{
											messagePlayerColor(parent->skill[2], color, language[398], language[2000 + (hitstats->type - KOBOLD)]);
										}
									}
								}
							}
							spawnMagicEffectParticles(hit.entity->x, hit.entity->y, hit.entity->z, my->sprite);
						}
					}
				}
				else if (!strcmp(element->name, spellElement_lightning.name))
				{
					playSoundEntity(my, 173, 128);
					if (hit.entity)
					{
						if (hit.entity->behavior == &actMonster || hit.entity->behavior == &actPlayer)
						{
							Entity* parent = uidToEntity(my->parent);
							if ( !(svFlags & SV_FLAG_FRIENDLYFIRE) )
							{
								// test for friendly fire
								if ( parent && parent->checkFriend(hit.entity) )
								{
									if ( my->light != NULL )
									{
										list_RemoveNode(my->light->node);
										my->light = NULL;
									}
									list_RemoveNode(my->mynode);
									return;
								}
							}
							playSoundEntity(my, 173, 64);
							playSoundEntity(hit.entity, 28, 128);
							int damage = element->damage;
							//damage += ((element->mana - element->base_mana) / static_cast<double>(element->overload_multiplier)) * element->damage;
							damage *= damagetables[hitstats->type][5];
							damage /= (1 + (int)resistance);
							hit.entity->modHP(-damage);

							// write the obituary
							if (parent)
							{
								parent->killedByMonsterObituary(hit.entity);
							}

							// update enemy bar for attacker
							if ( !strcmp(hitstats->name, "") )
							{
								if ( hitstats->type < KOBOLD ) //Original monster count
								{
									updateEnemyBar(parent, hit.entity, language[90 + hitstats->type], hitstats->HP, hitstats->MAXHP);
								}
								else if ( hitstats->type >= KOBOLD ) //New monsters
								{
									updateEnemyBar(parent, hit.entity, language[2000 + (hitstats->type - KOBOLD)], hitstats->HP, hitstats->MAXHP);
								}
							}
							else
							{
								updateEnemyBar(parent, hit.entity, hitstats->name, hitstats->HP, hitstats->MAXHP);
							}
							if ( hitstats->HP <= 0 && parent)
							{
								parent->awardXP( hit.entity, true, true );
							}
						}
						else if (hit.entity->behavior == &actDoor)
						{
							int damage = element->damage;
							//damage += ((element->mana - element->base_mana) / static_cast<double>(element->overload_multiplier)) * element->damage;
							damage /= (1 + (int)resistance);

							hit.entity->doorHandleDamageMagic(damage, *my, parent);

							my->removeLightField();
							list_RemoveNode(my->mynode);
							return;
							/*} else if (hit.entity->behavior == &actChest) { //TODO: Get right skill values and language file entries.
								int damage = element->damage;
								//damage += ((element->mana - element->base_mana) / static_cast<double>(element->overload_multiplier)) * element->damage;
								damage /= (1+(int)resistance);

								hit.entity->skill[3] -= damage; //Decrease chest health.
								if( hit.entity->skill[3] < 0 ) {
									if( parent ) {
										if( parent->behavior == &actPlayer ) {
											messagePlayer(parent->skill[2],language[387]);
										}
									}
								}
								playSoundEntity(hit.entity, 28, 128);
								if( !hit.entity->skill[0] )
									hit.entity->skill[6] = (my->x > hit.entity->x);
								else
									hit.entity->skill[6] = (my->y < hit.entity->y);
								my->removeLightField();
								updateEnemyBar(parent,hit.entity,language[674],hit.entity->skill[3],hit.entity->skill[9]);
								list_RemoveNode(my->mynode);
								return;*/
						}
						else if (hit.entity->behavior == &actFurniture )
						{
							int damage = element->damage;
							damage /= (1 + (int)resistance);
							hit.entity->furnitureHealth -= damage;
							if ( hit.entity->furnitureHealth < 0 )
							{
								if ( parent )
								{
									if ( parent->behavior == &actPlayer )
									{
										switch ( hit.entity->furnitureType )
										{
											case FURNITURE_CHAIR:
												messagePlayer(parent->skill[2], language[388]);
												updateEnemyBar(parent, hit.entity, language[677], hit.entity->furnitureHealth, hit.entity->furnitureMaxHealth);
												break;
											case FURNITURE_TABLE:
												messagePlayer(parent->skill[2], language[389]);
												updateEnemyBar(parent, hit.entity, language[676], hit.entity->furnitureHealth, hit.entity->furnitureMaxHealth);
												break;
											case FURNITURE_BED:
												messagePlayer(parent->skill[2], language[2508], language[2505]);
												updateEnemyBar(parent, hit.entity, language[2505], hit.entity->furnitureHealth, hit.entity->furnitureMaxHealth);
												break;
											case FURNITURE_BUNKBED:
												messagePlayer(parent->skill[2], language[2508], language[2506]);
												updateEnemyBar(parent, hit.entity, language[2506], hit.entity->furnitureHealth, hit.entity->furnitureMaxHealth);
												break;
											case FURNITURE_PODIUM:
												messagePlayer(parent->skill[2], language[2508], language[2507]);
												updateEnemyBar(parent, hit.entity, language[2507], hit.entity->furnitureHealth, hit.entity->furnitureMaxHealth);
												break;
											default:
												break;
										}
									}
								}
							}
							playSoundEntity(hit.entity, 28, 128);
						}
					}
				}
				else if (!strcmp(element->name, spellElement_locking.name))
				{
					if ( hit.entity )
					{
						if (hit.entity->behavior == &actDoor)
						{
							playSoundEntity(hit.entity, 92, 64);
							hit.entity->skill[5] = 1; //Lock the door.
							if ( parent )
							{
								if ( parent->behavior == &actPlayer )
								{
									messagePlayer(parent->skill[2], language[399]);
								}
							}
						}
						else if (hit.entity->behavior == &actChest)
						{
							//Lock chest
							playSoundEntity(hit.entity, 92, 64);
							if ( !hit.entity->chestLocked )
							{
								hit.entity->lockChest();
								if ( parent )
								{
									if ( parent->behavior == &actPlayer )
									{
										messagePlayer(parent->skill[2], language[400]);
									}
								}
							}
						}
						else
						{
							if ( parent )
								if ( parent->behavior == &actPlayer )
								{
									messagePlayer(parent->skill[2], language[401]);
								}
							if ( player >= 0 )
							{
								messagePlayer(player, language[402]);
							}
						}
						spawnMagicEffectParticles(hit.entity->x, hit.entity->y, hit.entity->z, my->sprite);
					}
				}
				else if (!strcmp(element->name, spellElement_opening.name))
				{
					if (hit.entity)
					{
						if (hit.entity->behavior == &actDoor)
						{
							// Open the Door
							playSoundEntity(hit.entity, 91, 64); // "UnlockDoor.ogg"
							hit.entity->skill[5] = 0; // Unlocks the Door

							if ( !hit.entity->skill[0] && !hit.entity->skill[3] )
							{
								hit.entity->skill[3] = 1 + (my->x > hit.entity->x); // Opens the Door
								playSoundEntity(hit.entity, 21, 96); // "UnlockDoor.ogg"
							}
							else if ( hit.entity->skill[0] && !hit.entity->skill[3] )
							{
								hit.entity->skill[3] = 1 + (my->x < hit.entity->x); // Opens the Door
								playSoundEntity(hit.entity, 21, 96); // "UnlockDoor.ogg"
							}
							if ( parent )
								if ( parent->behavior == &actPlayer)
								{
									messagePlayer(parent->skill[2], language[402]);
								}
						}
						else if ( hit.entity->behavior == &actGate )
						{
							// Open the Gate
							if ( hit.entity->skill[28] != 2 )
							{
								hit.entity->skill[28] = 2; // Powers the Gate
								if ( parent )
								{
									if ( parent->behavior == &actPlayer )
									{
										messagePlayer(parent->skill[2], language[403]); // "The spell opens the gate!"
									}
								}
							}
						}
						else if ( hit.entity->behavior == &actChest )
						{
							// Unlock the Chest
							if ( hit.entity->chestLocked )
							{
								playSoundEntity(hit.entity, 91, 64); // "UnlockDoor.ogg"
								hit.entity->unlockChest();
								if ( parent )
								{
									if ( parent->behavior == &actPlayer)
									{
										messagePlayer(parent->skill[2], language[404]); // "The spell unlocks the chest!"
									}
								}
							}
						}
						else if ( hit.entity->behavior == &actPowerCrystalBase )
						{
							Entity* childentity = static_cast<Entity*>((&hit.entity->children)->first->element);
							if ( childentity != nullptr )
							{

								//Unlock crystal
								if ( childentity->crystalSpellToActivate )
								{
									playSoundEntity(hit.entity, 151, 128);
									childentity->crystalSpellToActivate = 0;
									// send the clients the updated skill.
									serverUpdateEntitySkill(childentity, 10);
									if ( parent )
									{
										if ( parent->behavior == &actPlayer )
										{
											messagePlayer(parent->skill[2], language[2358]);
										}
									}
								}
							}
						}
						else
						{
							if ( parent )
							{
								if ( parent->behavior == &actPlayer )
								{
									messagePlayer(parent->skill[2], language[401]); // "No telling what it did..."
								}
							}

							if ( player >= 0 )
							{
								messagePlayer(player, language[401]); // "No telling what it did..."
							}
						}

						spawnMagicEffectParticles(hit.entity->x, hit.entity->y, hit.entity->z, my->sprite);
					}
				}
				else if (!strcmp(element->name, spellElement_dig.name))
				{
					if ( !hit.entity )
					{
						if ( hit.mapx >= 1 && hit.mapx < map.width - 1 && hit.mapy >= 1 && hit.mapy < map.height - 1 )
						{
							if ( map.tiles[(int)(OBSTACLELAYER + hit.mapy * MAPLAYERS + hit.mapx * MAPLAYERS * map.height)] != 0 )
							{
								if ( parent && parent->behavior == &actPlayer && MFLAG_DISABLEDIGGING )
								{
									Uint32 color = SDL_MapRGB(mainsurface->format, 255, 0, 255);
									messagePlayerColor(parent->skill[2], color, language[2380]); // disabled digging.
								}
								else
								{
									playSoundEntity(my, 66, 128);
									playSoundEntity(my, 67, 128);

									// spawn several rock items //TODO: This should really be its own function.
									i = 8 + rand() % 4;
									for ( c = 0; c < i; c++ )
									{
										entity = newEntity(-1, 1, map.entities);
										entity->flags[INVISIBLE] = true;
										entity->flags[UPDATENEEDED] = true;
										entity->x = hit.mapx * 16 + 4 + rand() % 8;
										entity->y = hit.mapy * 16 + 4 + rand() % 8;
										entity->z = -6 + rand() % 12;
										entity->sizex = 4;
										entity->sizey = 4;
										entity->yaw = rand() % 360 * PI / 180;
										entity->vel_x = (rand() % 20 - 10) / 10.0;
										entity->vel_y = (rand() % 20 - 10) / 10.0;
										entity->vel_z = -.25 - (rand() % 5) / 10.0;
										entity->flags[PASSABLE] = true;
										entity->behavior = &actItem;
										entity->flags[USERFLAG1] = true; // no collision: helps performance
										entity->skill[10] = GEM_ROCK;    // type
										entity->skill[11] = WORN;        // status
										entity->skill[12] = 0;           // beatitude
										entity->skill[13] = 1;           // count
										entity->skill[14] = 0;           // appearance
										entity->skill[15] = false;       // identified
									}

									map.tiles[(int)(OBSTACLELAYER + hit.mapy * MAPLAYERS + hit.mapx * MAPLAYERS * map.height)] = 0;
									// send wall destroy info to clients
									for ( c = 1; c < MAXPLAYERS; c++ )
									{
										if ( client_disconnected[c] == true )
										{
											continue;
										}
										strcpy((char*)net_packet->data, "WALD");
										SDLNet_Write16((Uint16)hit.mapx, &net_packet->data[4]);
										SDLNet_Write16((Uint16)hit.mapy, &net_packet->data[6]);
										net_packet->address.host = net_clients[c - 1].host;
										net_packet->address.port = net_clients[c - 1].port;
										net_packet->len = 8;
										sendPacketSafe(net_sock, -1, net_packet, c - 1);
									}
								}
							}
						}
					}
					else
					{
						if ( hit.entity->behavior == &actBoulder )
						{
							int i = 8 + rand() % 4;

							int c;
							for ( c = 0; c < i; c++ )
							{
								Entity* entity = newEntity(-1, 1, map.entities);
								entity->flags[INVISIBLE] = true;
								entity->flags[UPDATENEEDED] = true;
								entity->x = hit.entity->x - 4 + rand() % 8;
								entity->y = hit.entity->y - 4 + rand() % 8;
								entity->z = -6 + rand() % 12;
								entity->sizex = 4;
								entity->sizey = 4;
								entity->yaw = rand() % 360 * PI / 180;
								entity->vel_x = (rand() % 20 - 10) / 10.0;
								entity->vel_y = (rand() % 20 - 10) / 10.0;
								entity->vel_z = -.25 - (rand() % 5) / 10.0;
								entity->flags[PASSABLE] = true;
								entity->behavior = &actItem;
								entity->flags[USERFLAG1] = true; // no collision: helps performance
								entity->skill[10] = GEM_ROCK;    // type
								entity->skill[11] = WORN;        // status
								entity->skill[12] = 0;           // beatitude
								entity->skill[13] = 1;           // count
								entity->skill[14] = 0;           // appearance
								entity->skill[15] = false;       // identified
							}

							double ox = hit.entity->x;
							double oy = hit.entity->y;

							// destroy the boulder
							playSoundEntity(hit.entity, 67, 128);
							list_RemoveNode(hit.entity->mynode);
							if ( parent )
								if ( parent->behavior == &actPlayer )
								{
									messagePlayer(parent->skill[2], language[405]);
								}

							// on sokoban, destroying boulders spawns scorpions
							if ( !strcmp(map.name, "Sokoban") )
							{
								Entity* monster = summonMonster(SCORPION, ox, oy);
								if ( monster )
								{
									int c;
									for ( c = 0; c < MAXPLAYERS; c++ )
									{
										Uint32 color = SDL_MapRGB(mainsurface->format, 255, 128, 0);
										messagePlayerColor(c, color, language[406]);
									}
								}
							}
						}
						else
						{
							if ( parent )
								if ( parent->behavior == &actPlayer )
								{
									messagePlayer(parent->skill[2], language[401]);
								}
							if ( player >= 0 )
							{
								messagePlayer(player, language[401]);
							}
						}
					}
				}
				else if ( !strcmp(element->name, spellElement_stoneblood.name) )
				{
					if ( hit.entity )
					{
						if ( hit.entity->behavior == &actMonster || hit.entity->behavior == &actPlayer )
						{
							if ( !(svFlags & SV_FLAG_FRIENDLYFIRE) )
							{
								// test for friendly fire
								if ( parent && parent->checkFriend(hit.entity) )
								{
									if ( my->light != NULL )
									{
										list_RemoveNode(my->light->node);
										my->light = NULL;
									}
									list_RemoveNode(my->mynode);
									return;
								}
							}
							playSoundEntity(hit.entity, 172, 64); //TODO: Paralyze spell sound.
							hitstats->EFFECTS[EFF_PARALYZED] = true;
							hitstats->EFFECTS_TIMERS[EFF_PARALYZED] = (element->duration * (((element->mana) / static_cast<double>(element->base_mana)) * element->overload_multiplier));
							hitstats->EFFECTS_TIMERS[EFF_PARALYZED] /= (1 + (int)resistance);
							if ( hit.entity->behavior == &actPlayer )
							{
								serverUpdateEffects(hit.entity->skill[2]);
							}
							// update enemy bar for attacker
							if ( parent )
							{
								Uint32 color = SDL_MapRGB(mainsurface->format, 0, 255, 0);
								if ( parent->behavior == &actPlayer )
								{
									if ( strcmp(hitstats->name, "") )
									{
										messagePlayerColor(parent->skill[2], color, language[2420], hitstats->name);
									}
									else
									{
										if ( hitstats->type < KOBOLD ) //Original monster count
										{
											messagePlayerColor(parent->skill[2], color, language[2421], language[90 + hitstats->type]);
										}
										else if ( hitstats->type >= KOBOLD ) //New monsters
										{
											messagePlayerColor(parent->skill[2], color, language[2421], language[2000 + (hitstats->type - KOBOLD)]);
										}
									}
								}
							}

							Uint32 color = SDL_MapRGB(mainsurface->format, 255, 0, 0);
							if ( player >= 0 )
							{
								messagePlayerColor(player, color, language[2422]);
							}
							spawnMagicEffectParticles(hit.entity->x, hit.entity->y, hit.entity->z, my->sprite);
						}
					}
				}
				else if ( !strcmp(element->name, spellElement_bleed.name) )
				{
					playSoundEntity(my, 173, 128);
					if ( hit.entity )
					{
						if ( hit.entity->behavior == &actMonster || hit.entity->behavior == &actPlayer )
						{
							Entity* parent = uidToEntity(my->parent);
							if ( !(svFlags & SV_FLAG_FRIENDLYFIRE) )
							{
								// test for friendly fire
								if ( parent && parent->checkFriend(hit.entity) )
								{
									my->removeLightField();
									list_RemoveNode(my->mynode);
									return;
								}
							}
							playSoundEntity(my, 173, 64);
							playSoundEntity(hit.entity, 28, 128);
							int damage = element->damage;
							//damage += ((element->mana - element->base_mana) / static_cast<double>(element->overload_multiplier)) * element->damage;
							damage *= damagetables[hitstats->type][5];
							damage /= (1 + (int)resistance);
							hit.entity->modHP(-damage);

							// write the obituary
							if ( parent )
							{
								parent->killedByMonsterObituary(hit.entity);
							}

							hitstats->EFFECTS[EFF_BLEEDING] = true;
							hitstats->EFFECTS_TIMERS[EFF_BLEEDING] = (element->duration * (((element->mana) / static_cast<double>(element->base_mana)) * element->overload_multiplier));
							hitstats->EFFECTS_TIMERS[EFF_BLEEDING] /= (1 + (int)resistance);
							hitstats->EFFECTS[EFF_SLOW] = true;
							hitstats->EFFECTS_TIMERS[EFF_SLOW] = (element->duration * (((element->mana) / static_cast<double>(element->base_mana)) * element->overload_multiplier));
							hitstats->EFFECTS_TIMERS[EFF_SLOW] /= 4;
							hitstats->EFFECTS_TIMERS[EFF_SLOW] /= (1 + (int)resistance);
							if ( hit.entity->behavior == &actPlayer )
							{
								serverUpdateEffects(hit.entity->skill[2]);
							}
							// update enemy bar for attacker
							if ( parent )
							{
								Uint32 color = SDL_MapRGB(mainsurface->format, 0, 255, 0);
								if ( parent->behavior == &actPlayer )
								{
									if ( strcmp(hitstats->name, "") )
									{
										messagePlayerColor(parent->skill[2], color, language[2423], hitstats->name);
									}
									else
									{
										if ( hitstats->type < KOBOLD ) //Original monster count
										{
											messagePlayerColor(parent->skill[2], color, language[2424], language[90 + hitstats->type]);
										}
										else if ( hitstats->type >= KOBOLD ) //New monsters
										{
											messagePlayerColor(parent->skill[2], color, language[2424], language[2000 + (hitstats->type - KOBOLD)]);
										}
									}
								}
							}

							// write the obituary
							if ( parent )
							{
								parent->killedByMonsterObituary(hit.entity);
							}

							// update enemy bar for attacker
							if ( !strcmp(hitstats->name, "") )
							{
								if ( hitstats->type < KOBOLD ) //Original monster count
								{
									updateEnemyBar(parent, hit.entity, language[90 + hitstats->type], hitstats->HP, hitstats->MAXHP);
								}
								else if ( hitstats->type >= KOBOLD ) //New monsters
								{
									updateEnemyBar(parent, hit.entity, language[2000 + (hitstats->type - KOBOLD)], hitstats->HP, hitstats->MAXHP);
								}
							}
							else
							{
								updateEnemyBar(parent, hit.entity, hitstats->name, hitstats->HP, hitstats->MAXHP);
							}

							if ( hitstats->HP <= 0 && parent )
							{
								parent->awardXP(hit.entity, true, true);
							}

							Uint32 color = SDL_MapRGB(mainsurface->format, 255, 0, 0);
							if ( player >= 0 )
							{
								messagePlayerColor(player, color, language[2425]);
							}
						}
					}
				}
				else if ( !strcmp(element->name, spellElement_dominate.name) )
				{
					Entity *caster = uidToEntity(spell->caster);
					if ( caster )
					{
						if ( spellEffectDominate(*my, *element, *caster, parent) )
						{
							//Success
						}
					}
				}
				else if ( !strcmp(element->name, spellElement_acidSpray.name) )
				{
					spellEffectAcid(*my, *element, parent, resistance);
				}
				else if ( !strcmp(element->name, spellElement_stealWeapon.name) )
				{
					spellEffectStealWeapon(*my, *element, parent, resistance);
				}
				else if ( !strcmp(element->name, spellElement_drainSoul.name) )
				{
					spellEffectDrainSoul(*my, *element, parent, resistance);
				}

				if ( hitstats && player >= 0 )
				{
					entityHealth -= hitstats->HP;
					if ( entityHealth > 0 )
					{
						// entity took damage, shake screen.
						if ( multiplayer == SERVER && player > 0 )
						{
							strcpy((char*)net_packet->data, "SHAK");
							net_packet->data[4] = 10; // turns into .1
							net_packet->data[5] = 10;
							net_packet->address.host = net_clients[player - 1].host;
							net_packet->address.port = net_clients[player - 1].port;
							net_packet->len = 6;
							sendPacketSafe(net_sock, -1, net_packet, player - 1);
						}
						else if ( player == 0 )
						{
							camera_shakex += .1;
							camera_shakey += 10;
						}
					}
				}

				my->removeLightField();
				if ( my->mynode )
				{
					list_RemoveNode(my->mynode);
				}
				return;
			}
		}

		//Go down two levels to the next element. This will need to get re-written shortly.
		node = spell->elements.first;
		element = (spellElement_t*)node->element;
		//element = (spellElement_t *)spell->elements->first->element;
		//element = (spellElement_t *)element->elements->first->element; //Go down two levels to the second element.
		node = element->elements.first;
		element = (spellElement_t*)node->element;
		if (!strcmp(element->name, spellElement_fire.name) || !strcmp(element->name, spellElement_lightning.name))
		{
			//Make the ball light up stuff as it travels.
			my->light = lightSphereShadow(my->x / 16, my->y / 16, 8, 192);
			lightball_flicker++;
			my->skill[2] = -11; // so clients know to create a light field

			if (lightball_flicker > 5)
			{
				lightball_lighting = (lightball_lighting == 1) + 1;

				if (lightball_lighting == 1)
				{
					my->removeLightField();
					my->light = lightSphereShadow(my->x / 16, my->y / 16, 8, 192);
				}
				else
				{
					my->removeLightField();
					my->light = lightSphereShadow(my->x / 16, my->y / 16, 8, 174);
				}
				lightball_flicker = 0;
			}
		}
		else
		{
			my->skill[2] = -12; // so clients know to simply spawn particles
		}

		// spawn particles
		spawnMagicParticle(my);
	}
	else
	{
		//Any init stuff that needs to happen goes here.
		magic_init = 1;
		my->skill[2] = -7; // ordinarily the client won't do anything with this entity
	}
}

void actMagicClient(Entity* my)
{
	my->removeLightField();
	my->light = lightSphereShadow(my->x / 16, my->y / 16, 8, 192);
	lightball_flicker++;
	my->skill[2] = -11; // so clients know to create a light field

	if (lightball_flicker > 5)
	{
		lightball_lighting = (lightball_lighting == 1) + 1;

		if (lightball_lighting == 1)
		{
			my->removeLightField();
			my->light = lightSphereShadow(my->x / 16, my->y / 16, 8, 192);
		}
		else
		{
			my->removeLightField();
			my->light = lightSphereShadow(my->x / 16, my->y / 16, 8, 174);
		}
		lightball_flicker = 0;
	}

	// spawn particles
	spawnMagicParticle(my);
}

void actMagicClientNoLight(Entity* my)
{
	spawnMagicParticle(my); // simply spawn particles
}

void actMagicParticle(Entity* my)
{
	my->x += my->vel_x;
	my->y += my->vel_y;
	my->z += my->vel_z;
	my->scalex -= 0.05;
	my->scaley -= 0.05;
	my->scalez -= 0.05;
	if ( my->scalex <= 0 )
	{
		my->scalex = 0;
		my->scaley = 0;
		my->scalez = 0;
		list_RemoveNode(my->mynode);
		return;
	}
}

Entity* spawnMagicParticle(Entity* parentent)
{
	Entity* entity;

	entity = newEntity(parentent->sprite, 1, map.entities);

	entity->x = parentent->x + (rand() % 50 - 25) / 20.f;
	entity->y = parentent->y + (rand() % 50 - 25) / 20.f;
	entity->z = parentent->z + (rand() % 50 - 25) / 20.f;
	entity->scalex = 0.7;
	entity->scaley = 0.7;
	entity->scalez = 0.7;
	entity->sizex = 1;
	entity->sizey = 1;
	entity->yaw = parentent->yaw;
	entity->pitch = parentent->pitch;
	entity->roll = parentent->roll;
	entity->flags[NOUPDATE] = true;
	entity->flags[PASSABLE] = true;
	entity->flags[BRIGHT] = true;
	entity->flags[UNCLICKABLE] = true;
	entity->flags[NOUPDATE] = true;
	entity->flags[UPDATENEEDED] = false;
	entity->behavior = &actMagicParticle;
	if ( multiplayer != CLIENT )
	{
		entity_uids--;
	}
	entity->setUID(-3);

	return entity;
}

void spawnMagicEffectParticles(Sint16 x, Sint16 y, Sint16 z, Uint32 sprite)
{
	int c;
	if ( multiplayer == SERVER )
	{
		for ( c = 1; c < MAXPLAYERS; c++ )
		{
			if ( client_disconnected[c] )
			{
				continue;
			}
			strcpy((char*)net_packet->data, "MAGE");
			SDLNet_Write16(x, &net_packet->data[4]);
			SDLNet_Write16(y, &net_packet->data[6]);
			SDLNet_Write16(z, &net_packet->data[8]);
			SDLNet_Write32(sprite, &net_packet->data[10]);
			net_packet->address.host = net_clients[c - 1].host;
			net_packet->address.port = net_clients[c - 1].port;
			net_packet->len = 14;
			sendPacketSafe(net_sock, -1, net_packet, c - 1);
		}
	}

	// boosty boost
	for ( c = 0; c < 10; c++ )
	{
		Entity* entity = newEntity(sprite, 1, map.entities);
		entity->x = x - 5 + rand() % 11;
		entity->y = y - 5 + rand() % 11;
		entity->z = z - 10 + rand() % 21;
		entity->scalex = 0.7;
		entity->scaley = 0.7;
		entity->scalez = 0.7;
		entity->sizex = 1;
		entity->sizey = 1;
		entity->yaw = (rand() % 360) * PI / 180.f;
		entity->flags[PASSABLE] = true;
		entity->flags[BRIGHT] = true;
		entity->flags[NOUPDATE] = true;
		entity->flags[UNCLICKABLE] = true;
		entity->behavior = &actMagicParticle;
		entity->vel_z = -1;
		if ( multiplayer != CLIENT )
		{
			entity_uids--;
		}
		entity->setUID(-3);
	}
}

void createParticle1(Entity* caster, int player)
{
	Entity* entity = newEntity(-1, 1, map.entities);
	entity->sizex = 0;
	entity->sizey = 0;
	entity->x = caster->x;
	entity->y = caster->y;
	entity->z = -7;
	entity->vel_z = 0.3;
	entity->yaw = (rand() % 360) * PI / 180.0;
	entity->skill[0] = 50;
	entity->skill[1] = player;
	entity->fskill[0] = 0.03;
	entity->light = lightSphereShadow(entity->x / 16, entity->y / 16, 3, 192);
	entity->behavior = &actParticleCircle;
	entity->flags[PASSABLE] = true;
	entity->flags[INVISIBLE] = true;
	entity->setUID(-3);

}

void createParticleCircling(Entity* parent, int duration, int sprite)
{
	Entity* entity = newEntity(sprite, 1, map.entities);
	entity->sizex = 1;
	entity->sizey = 1;
	entity->x = parent->x;
	entity->y = parent->y;
	entity->focalx = 8;
	entity->z = -7;
	entity->vel_z = 0.15;
	entity->yaw = (rand() % 360) * PI / 180.0;
	entity->skill[0] = duration;
	entity->skill[1] = -1;
	//entity->scalex = 0.01;
	//entity->scaley = 0.01;
	entity->fskill[0] = -0.1;
	entity->behavior = &actParticleCircle;
	entity->flags[PASSABLE] = true;
	entity->setUID(-3);

	real_t tmp = entity->yaw;

	entity = newEntity(sprite, 1, map.entities);
	entity->sizex = 1;
	entity->sizey = 1;
	entity->x = parent->x;
	entity->y = parent->y;
	entity->focalx = 8;
	entity->z = -7;
	entity->vel_z = 0.15;
	entity->yaw = tmp + (2 * PI / 3);
	entity->particleDuration = duration;
	entity->skill[1] = -1;
	//entity->scalex = 0.01;
	//entity->scaley = 0.01;
	entity->fskill[0] = -0.1;
	entity->behavior = &actParticleCircle;
	entity->flags[PASSABLE] = true;
	entity->setUID(-3);

	entity = newEntity(sprite, 1, map.entities);
	entity->sizex = 1;
	entity->sizey = 1;
	entity->x = parent->x;
	entity->y = parent->y;
	entity->focalx = 8;
	entity->z = -7;
	entity->vel_z = 0.15;
	entity->yaw = tmp - (2 * PI / 3);
	entity->particleDuration = duration;
	entity->skill[1] = -1;
	//entity->scalex = 0.01;
	//entity->scaley = 0.01;
	entity->fskill[0] = -0.1;
	entity->behavior = &actParticleCircle;
	entity->flags[PASSABLE] = true;
	entity->setUID(-3);

	entity = newEntity(sprite, 1, map.entities);
	entity->sizex = 1;
	entity->sizey = 1;
	entity->x = parent->x;
	entity->y = parent->y;
	entity->focalx = 16;
	entity->z = -12;
	entity->vel_z = 0.2;
	entity->yaw = tmp;
	entity->particleDuration = duration;
	entity->skill[1] = -1;
	//entity->scalex = 0.01;
	//entity->scaley = 0.01;
	entity->fskill[0] = 0.1;
	entity->behavior = &actParticleCircle;
	entity->flags[PASSABLE] = true;
	entity->setUID(-3);

	entity = newEntity(sprite, 1, map.entities);
	entity->sizex = 1;
	entity->sizey = 1;
	entity->x = parent->x;
	entity->y = parent->y;
	entity->focalx = 16;
	entity->z = -12;
	entity->vel_z = 0.2;
	entity->yaw = tmp + (2 * PI / 3);
	entity->particleDuration = duration;
	entity->skill[1] = -1;
	//entity->scalex = 0.01;
	//entity->scaley = 0.01;
	entity->fskill[0] = 0.1;
	entity->behavior = &actParticleCircle;
	entity->flags[PASSABLE] = true;
	entity->setUID(-3);

	entity = newEntity(sprite, 1, map.entities);
	entity->sizex = 1;
	entity->sizey = 1;
	entity->x = parent->x;
	entity->y = parent->y;
	entity->focalx = 16;
	entity->z = -12;
	entity->vel_z = 0.2;
	entity->yaw = tmp - (2 * PI / 3);
	entity->particleDuration = duration;
	entity->skill[1] = -1;
	//entity->scalex = 0.01;
	//entity->scaley = 0.01;
	entity->fskill[0] = 0.1;
	entity->behavior = &actParticleCircle;
	entity->flags[PASSABLE] = true;
	entity->setUID(-3);
}

#define PARTICLE_LIFE my->skill[0]
#define PARTICLE_CASTER my->skill[1]

void actParticleCircle(Entity* my)
{
	if ( PARTICLE_LIFE < 0 )
	{
		list_RemoveNode(my->mynode);
		return;
	}
	else
	{
		--PARTICLE_LIFE;
		my->yaw += my->fskill[0];
		if ( my->fskill[0] < 0.4 && my->fskill[0] > (-0.4) )
		{
			my->fskill[0] = my->fskill[0] * 1.05;
		}
		my->z += my->vel_z;
		if ( my->focalx > 0.05 )
		{
			if ( my->vel_z == 0.15 )
			{
				my->focalx = my->focalx * 0.97;
			}
			else
			{
				my->focalx = my->focalx * 0.97;
			}
		}
		my->scalex *= 0.995;
		my->scaley *= 0.995;
		my->scalez *= 0.995;
	}
}

void createParticleDot(Entity* parent)
{
	for ( int c = 0; c < 50; c++ )
	{
		Entity* entity = newEntity(576, 1, map.entities);
		entity->sizex = 1;
		entity->sizey = 1;
		entity->x = parent->x + (-4 + rand() % 9);
		entity->y = parent->y + (-4 + rand() % 9);
		entity->z = 7.5 + rand()%50;
		entity->vel_z = -1;
		//entity->yaw = (rand() % 360) * PI / 180.0;
		entity->skill[0] = 10 + rand()% 50;
		entity->behavior = &actParticleDot;
		entity->flags[PASSABLE] = true;
		entity->flags[NOUPDATE] = true;
		entity->flags[UNCLICKABLE] = true;
		if ( multiplayer != CLIENT )
		{
			entity_uids--;
		}
		entity->setUID(-3);
	}
}

void createParticleRock(Entity* parent)
{
	for ( int c = 0; c < 5; c++ )
	{
		Entity* entity = newEntity(78, 1, map.entities);
		entity->sizex = 1;
		entity->sizey = 1;
		entity->x = parent->x + (-4 + rand() % 9);
		entity->y = parent->y + (-4 + rand() % 9);
		entity->z = 7.5;
		entity->yaw = c * 2 * PI / 5;//(rand() % 360) * PI / 180.0;
		entity->roll = (rand() % 360) * PI / 180.0;

		entity->vel_x = 0.2 * cos(entity->yaw);
		entity->vel_y = 0.2 * sin(entity->yaw);
		entity->vel_z = 3;// 0.25 - (rand() % 5) / 10.0;

		entity->skill[0] = 50; // particle life
		entity->skill[1] = 0; // particle direction, 0 = upwards, 1 = downwards.

		entity->behavior = &actParticleRock;
		entity->flags[PASSABLE] = true;
		entity->flags[NOUPDATE] = true;
		entity->flags[UNCLICKABLE] = true;
		if ( multiplayer != CLIENT )
		{
			entity_uids--;
		}
		entity->setUID(-3);
	}
}

void actParticleRock(Entity* my)
{
	if ( PARTICLE_LIFE < 0 || my->z > 10 )
	{
		list_RemoveNode(my->mynode);
	}
	else
	{
		--PARTICLE_LIFE;
		my->x += my->vel_x;
		my->y += my->vel_y;

		my->roll += 0.1;

		if ( my->vel_z < 0.01 )
		{
			my->skill[1] = 1; // start moving downwards
			my->vel_z = 0.1;
		}

		if ( my->skill[1] == 0 ) // upwards motion
		{
			my->z -= my->vel_z;
			my->vel_z *= 0.7;
		}
		else // downwards motion
		{
			my->z += my->vel_z;
			my->vel_z *= 1.1;
		}
	}
	return;
}

void actParticleDot(Entity* my)
{
	if ( PARTICLE_LIFE < 0 )
	{
		list_RemoveNode(my->mynode);
	}
	else
	{
		--PARTICLE_LIFE;
		my->z += my->vel_z;
		//my->z -= 0.01;
	}
	return;
}

void actParticleTest(Entity* my)
{
	if ( PARTICLE_LIFE < 0 )
	{
		list_RemoveNode(my->mynode);
		return;
	}
	else
	{
		--PARTICLE_LIFE;
		my->x += my->vel_x;
		my->y += my->vel_y;
		my->z += my->vel_z;
		//my->z -= 0.01;
	}
}

void createParticleErupt(Entity* parent, int sprite)
{
	real_t yaw = 0;
	int numParticles = 8;
	for ( int c = 0; c < 8; c++ )
	{
		Entity* entity = newEntity(sprite, 1, map.entities);
		entity->sizex = 1;
		entity->sizey = 1;
		entity->x = parent->x;
		entity->y = parent->y;
		entity->z = 7.5; // start from the ground.
		entity->yaw = yaw;
		entity->vel_x = 0.2;
		entity->vel_y = 0.2;
		entity->vel_z = -2;
		entity->skill[0] = 100;
		entity->skill[1] = 0; // direction.
		entity->fskill[0] = 0.1;
		entity->behavior = &actParticleErupt;
		entity->flags[PASSABLE] = true;
		entity->flags[NOUPDATE] = true;
		entity->flags[UNCLICKABLE] = true;
		if ( multiplayer != CLIENT )
		{
			entity_uids--;
		}
		entity->setUID(-3);
		yaw += 2 * PI / numParticles;
	}
}

Entity* createParticleSapCenter(Entity* parent, Entity* target, int spell, int sprite, int endSprite)
{
	if ( !parent || !target )
	{
		return nullptr;
	}
	// spawns the invisible 'center' of the magic particle
	Entity* entity = newEntity(sprite, 1, map.entities);
	entity->sizex = 1;
	entity->sizey = 1;
	entity->x = target->x;
	entity->y = target->y;
	entity->parent = (parent->getUID());
	entity->yaw = parent->yaw + PI; // face towards the caster.
	entity->skill[0] = 45;
	entity->skill[2] = -13; // so clients know my behavior.
	entity->skill[3] = 0; // init
	entity->skill[4] = sprite; // visible sprites.
	entity->skill[5] = endSprite; // sprite to spawn on return to caster.
	entity->skill[6] = spell;
	entity->behavior = &actParticleSapCenter;
	entity->flags[INVISIBLE] = true;
	entity->flags[PASSABLE] = true;
	entity->flags[UPDATENEEDED] = true;
	entity->flags[UNCLICKABLE] = true;
	return entity;
}

void createParticleSap(Entity* parent)
{
	real_t speed = 0.4;
	for ( int c = 0; c < 4; c++ )
	{
		// 4 particles, in an 'x' pattern around parent sprite.
		int sprite = parent->sprite;
		if ( parent->skill[6] == SPELL_STEAL_WEAPON || parent->skill[6] == SHADOW_SPELLCAST )
		{
			sprite = parent->sprite;
		}
		else if ( parent->skill[6] == SPELL_DRAIN_SOUL )
		{
			if ( c == 0 || c == 3 )
			{
				sprite = parent->sprite;
			}
			else
			{
				sprite = 599;
			}
		}
		else if ( multiplayer == CLIENT )
		{
			// client won't receive the sprite skill data in time, fix for this until a solution is found!
			if ( sprite == 598 )
			{
				if ( c == 0 || c == 3 )
				{
				// drain HP particle
					sprite = parent->sprite;
				}
				else
				{
					// drain MP particle
					sprite = 599;
				}
			}
		}
		Entity* entity = newEntity(sprite, 1, map.entities);
		entity->sizex = 1;
		entity->sizey = 1;
		entity->x = parent->x;
		entity->y = parent->y;
		entity->z = 0;
		entity->scalex = 0.9;
		entity->scaley = 0.9;
		entity->scalez = 0.9;
		if ( sprite == 598 || sprite == 599 )
		{
			entity->scalex = 0.5;
			entity->scaley = 0.5;
			entity->scalez = 0.5;
		}
		entity->parent = (parent->getUID());
		entity->yaw = parent->yaw;
		if ( c == 0 )
		{
			entity->vel_z = -speed;
			entity->vel_x = speed * cos(entity->yaw + PI / 2);
			entity->vel_y = speed * sin(entity->yaw + PI / 2);
			entity->yaw += PI / 3;
			entity->pitch -= PI / 6;
			entity->fskill[2] = -(PI / 3) / 25; // yaw rate of change.
			entity->fskill[3] = (PI / 6) / 25; // pitch rate of change.
		}
		else if ( c == 1 )
		{
			entity->vel_z = -speed;
			entity->vel_x = speed * cos(entity->yaw - PI / 2);
			entity->vel_y = speed * sin(entity->yaw - PI / 2);
			entity->yaw -= PI / 3;
			entity->pitch -= PI / 6;
			entity->fskill[2] = (PI / 3) / 25; // yaw rate of change.
			entity->fskill[3] = (PI / 6) / 25; // pitch rate of change.
		}
		else if ( c == 2 )
		{
			entity->vel_x = speed * cos(entity->yaw + PI / 2);
			entity->vel_y = speed * sin(entity->yaw + PI / 2);
			entity->vel_z = speed;
			entity->yaw += PI / 3;
			entity->pitch += PI / 6;
			entity->fskill[2] = -(PI / 3) / 25; // yaw rate of change.
			entity->fskill[3] = -(PI / 6) / 25; // pitch rate of change.
		}
		else if ( c == 3 )
		{
			entity->vel_x = speed * cos(entity->yaw - PI / 2);
			entity->vel_y = speed * sin(entity->yaw - PI / 2);
			entity->vel_z = speed;
			entity->yaw -= PI / 3;
			entity->pitch += PI / 6;
			entity->fskill[2] = (PI / 3) / 25; // yaw rate of change.
			entity->fskill[3] = -(PI / 6) / 25; // pitch rate of change.
		}

		entity->skill[3] = c; // particle index
		entity->fskill[0] = entity->vel_x; // stores the accumulated x offset from center
		entity->fskill[1] = entity->vel_y; // stores the accumulated y offset from center
		entity->skill[0] = 200; // lifetime
		entity->skill[1] = 0; // direction outwards
		entity->behavior = &actParticleSap;
		entity->flags[PASSABLE] = true;
		entity->flags[NOUPDATE] = true;
		if ( multiplayer != CLIENT )
		{
			entity_uids--;
		}
		entity->setUID(-3);
	}
}

void createParticleDropRising(Entity* parent, int sprite, double scale)
{
	for ( int c = 0; c < 50; c++ )
	{
		// shoot drops to the sky
		Entity* entity = newEntity(sprite, 1, map.entities);
		entity->sizex = 1;
		entity->sizey = 1;
		entity->x = parent->x - 4 + rand() % 9;
		entity->y = parent->y - 4 + rand() % 9;
		entity->z = 7.5 + rand() % 50;
		entity->vel_z = -1;
		//entity->yaw = (rand() % 360) * PI / 180.0;
		entity->particleDuration = 10 + rand() % 50;
		entity->scalex *= scale;
		entity->scaley *= scale;
		entity->scalez *= scale;
		entity->behavior = &actParticleDot;
		entity->flags[PASSABLE] = true;
		entity->flags[NOUPDATE] = true;
		entity->flags[UNCLICKABLE] = true;
		if ( multiplayer != CLIENT )
		{
			entity_uids--;
		}
		entity->setUID(-3);
	}
}

Entity* createParticleTimer(Entity* parent, int duration, int sprite)
{
	Entity* entity = newEntity(-1, 1, map.entities);
	entity->sizex = 1;
	entity->sizey = 1;
	entity->x = parent->x;
	entity->y = parent->y;
	entity->parent = (parent->getUID());
	entity->behavior = &actParticleTimer;
	entity->particleTimerDuration = duration;
	entity->flags[INVISIBLE] = true;
	entity->flags[PASSABLE] = true;
	entity->flags[NOUPDATE] = true;
	if ( multiplayer != CLIENT )
	{
		entity_uids--;
	}
	entity->setUID(-3);

	return entity;
}

void actParticleErupt(Entity* my)
{
	if ( PARTICLE_LIFE < 0 )
	{
		list_RemoveNode(my->mynode);
		return;
	}
	else
	{
		// particles jump up from the ground then back down again.
		--PARTICLE_LIFE;
		my->x += my->vel_x * cos(my->yaw);
		my->y += my->vel_y * sin(my->yaw);
		my->scalex *= 0.99;
		my->scaley *= 0.99;
		my->scalez *= 0.99;
		spawnMagicParticle(my);
		if ( my->skill[1] == 0 ) // rising
		{
			my->z += my->vel_z;
			my->vel_z *= 0.8;
			my->pitch = std::min<real_t>(my->pitch + my->fskill[0], PI / 2);
			my->fskill[0] = std::max<real_t>(my->fskill[0] * 0.85, 0.05);
			if ( my->vel_z > -0.02 )
			{
				my->skill[1] = 1;
			}
		}
		else // falling
		{
			my->pitch = std::min<real_t>(my->pitch + my->fskill[0], 15 * PI / 16);
			my->fskill[0] = std::min<real_t>(my->fskill[0] * (1 / 0.99), 0.1);
			my->z -= my->vel_z;
			my->vel_z *= (1 / 0.8);
			my->vel_z = std::max<real_t>(my->vel_z, -0.8);
		}
	}
}

void actParticleTimer(Entity* my)
{
	if( PARTICLE_LIFE < 0 )
	{
		if ( multiplayer != CLIENT )
		{
			if ( my->particleTimerEndAction == PARTICLE_EFFECT_INCUBUS_TELEPORT_STEAL )
			{
				// teleport to random location spell.
				Entity* parent = uidToEntity(my->parent);
				if ( parent )
				{
					createParticleErupt(parent, my->particleTimerEndSprite);
					if ( parent->teleportRandom() )
					{
						// teleport success.
						if ( multiplayer == SERVER )
						{
							serverSpawnMiscParticles(parent, PARTICLE_EFFECT_ERUPT, my->particleTimerEndSprite);
						}
					}
				}
			}
			else if ( my->particleTimerEndAction == PARTICLE_EFFECT_INCUBUS_TELEPORT_TARGET )
			{
				// teleport to target spell.
				Entity* parent = uidToEntity(my->parent);
				Entity* target = uidToEntity(static_cast<Uint32>(my->particleTimerTarget));
				if ( parent && target )
				{
					createParticleErupt(parent, my->particleTimerEndSprite);
					if ( parent->teleportAroundEntity(target, my->particleTimerVariable1) )
					{
						// teleport success.
						if ( multiplayer == SERVER )
						{
							serverSpawnMiscParticles(parent, PARTICLE_EFFECT_ERUPT, my->particleTimerEndSprite);
						}
					}
				}
			}
			else if ( my->particleTimerEndAction == PARTICLE_EFFECT_PORTAL_SPAWN )
			{
				Entity* parent = uidToEntity(my->parent);
				if ( parent )
				{
					parent->flags[INVISIBLE] = false;
					serverUpdateEntityFlag(parent, INVISIBLE);
					playSoundEntity(parent, 164, 128);
				}
				spawnExplosion(my->x, my->y, 0);
				my->removeLightField();
			}
			else if ( my->particleTimerEndAction == PARTICLE_EFFECT_SHADOW_TELEPORT )
			{
				// teleport to target spell.
				Entity* parent = uidToEntity(my->parent);
				if ( parent )
				{
					if ( parent->monsterSpecialState == SHADOW_TELEPORT_ONLY )
					{
						//messagePlayer(0, "Resetting shadow's monsterSpecialState!");
						parent->monsterSpecialState = 0;
						serverUpdateEntitySkill(parent, 33); // for clients to keep track of animation
					}
				}
				Entity* target = uidToEntity(static_cast<Uint32>(my->particleTimerTarget));
				if ( parent )
				{
					bool teleported = false;
					createParticleErupt(parent, my->particleTimerEndSprite);
					if ( target )
					{
						teleported = parent->teleportAroundEntity(target, my->particleTimerVariable1);
					}
					else
					{
						teleported = parent->teleportRandom();
					}

					if ( teleported )
					{
						// teleport success.
						if ( multiplayer == SERVER )
						{
							serverSpawnMiscParticles(parent, PARTICLE_EFFECT_ERUPT, my->particleTimerEndSprite);
						}
					}
				}
			}
		}
		list_RemoveNode(my->mynode);
		return;
	}
	else
	{
		--PARTICLE_LIFE;
		if ( my->particleTimerPreDelay <= 0 )
		{
			// shoot particles for the duration of the timer, centered at caster.
			if ( my->particleTimerCountdownAction == PARTICLE_TIMER_ACTION_SHOOT_PARTICLES )
			{
				Entity* parent = uidToEntity(my->parent);
				// shoot drops to the sky
				if ( parent && my->particleTimerCountdownSprite != 0 )
				{
					Entity* entity = newEntity(my->particleTimerCountdownSprite, 1, map.entities);
					entity->sizex = 1;
					entity->sizey = 1;
					entity->x = parent->x - 4 + rand() % 9;
					entity->y = parent->y - 4 + rand() % 9;
					entity->z = 7.5;
					entity->vel_z = -1;
					entity->yaw = (rand() % 360) * PI / 180.0;
					entity->particleDuration = 10 + rand() % 30;
					entity->behavior = &actParticleDot;
					entity->flags[PASSABLE] = true;
					entity->flags[NOUPDATE] = true;
					entity->flags[UNCLICKABLE] = true;
					if ( multiplayer != CLIENT )
					{
						entity_uids--;
					}
					entity->setUID(-3);
				}
			}
			// fire once off.
			else if ( my->particleTimerCountdownAction == PARTICLE_TIMER_ACTION_SPAWN_PORTAL )
			{
				Entity* parent = uidToEntity(my->parent);
				if ( parent && my->particleTimerCountdownAction < 100 )
				{
					playSoundEntityLocal(parent, 167, 128);
					createParticleDot(parent);
					createParticleCircling(parent, 100, my->particleTimerCountdownSprite);
					my->particleTimerCountdownAction = 0;
				}
			}
		}
		else
		{
			--my->particleTimerPreDelay;
		}
	}
}

void actParticleSap(Entity* my)
{
	real_t decel = 0.9;
	real_t accel = 0.9;
	real_t minSpeed = 0.05;

	if ( PARTICLE_LIFE < 0 )
	{
		list_RemoveNode(my->mynode);
		return;
	}
	else
	{
		spawnMagicParticle(my);
		Entity* parent = uidToEntity(my->parent);
		if ( parent )
		{
			my->x = parent->x + my->fskill[0];
			my->y = parent->y + my->fskill[1];
		}
		else
		{
			list_RemoveNode(my->mynode);
			return;
		}

		if ( my->skill[1] == 0 )
		{
			// move outwards diagonally.
			if ( abs(my->vel_z) > minSpeed )
			{
				my->fskill[0] += my->vel_x;
				my->fskill[1] += my->vel_y;
				my->vel_x *= decel;
				my->vel_y *= decel;

				my->z += my->vel_z;
				my->vel_z *= decel;

				my->yaw += my->fskill[2];
				my->pitch += my->fskill[3];
			}
			else
			{
				my->skill[1] = 1;
				my->vel_x *= -1;
				my->vel_y *= -1;
				my->vel_z *= -1;
			}
		}
		else if ( my->skill[1] == 1 )
		{
			// move inwards diagonally.
			if ( abs(my->vel_z) < 0.4 )
			{
				my->fskill[0] += my->vel_x;
				my->fskill[1] += my->vel_y;
				my->vel_x /= accel;
				my->vel_y /= accel;

				my->z += my->vel_z;
				my->vel_z /= accel;

				my->yaw += my->fskill[2];
				my->pitch += my->fskill[3];
			}
			else
			{
				// movement completed.
				my->skill[1] = 2;
			}
		}

		my->scalex *= 0.99;
		my->scaley *= 0.99;
		my->scalez *= 0.99;
		--PARTICLE_LIFE;
	}
}

void actParticleSapCenter(Entity* my)
{
	// init
	if ( my->skill[3] == 0 )
	{
		// for clients and server spawn the visible arcing particles.
		my->skill[3] = 1;
		createParticleSap(my);
	}

	if ( multiplayer == CLIENT )
	{
		return;
	}

	Entity* parent = uidToEntity(my->parent);
	if ( parent )
	{
		// if reached the caster, delete self and spawn some particles.
		if ( entityInsideEntity(my, parent) )
		{
			if ( my->skill[6] == SPELL_STEAL_WEAPON )
			{
				if ( my->skill[7] == 1 )
				{
					// found stolen item.
					Item* item = newItemFromEntity(my);
					if ( parent->behavior == &actPlayer )
					{
						itemPickup(parent->skill[2], item);
					}
					else if ( parent->behavior == &actMonster )
					{
						parent->addItemToMonsterInventory(item);
						Stat *myStats = parent->getStats();
						if ( myStats )
						{
							node_t* weaponNode = itemNodeInInventory(myStats, static_cast<ItemType>(-1), WEAPON);
							if ( weaponNode )
							{
								swapMonsterWeaponWithInventoryItem(parent, myStats, weaponNode, false, true);
								if ( myStats->type == INCUBUS )
								{
									parent->monsterSpecialState = INCUBUS_TELEPORT_STEAL;
									parent->monsterSpecialTimer = 100 + rand() % MONSTER_SPECIAL_COOLDOWN_INCUBUS_TELEPORT_RANDOM;
								}
							}
						}
					}
					item = nullptr;
				}
				playSoundEntity(parent, 168, 128);
				spawnMagicEffectParticles(parent->x, parent->y, parent->z, my->skill[5]);
			}
			else if ( my->skill[6] == SPELL_DRAIN_SOUL )
			{
				parent->modHP(my->skill[7]);
				parent->modMP(my->skill[8]);
				if ( parent->behavior == &actPlayer )
				{
					Uint32 color = SDL_MapRGB(mainsurface->format, 0, 255, 0);
					messagePlayerColor(parent->skill[2], color, language[2445]);
				}
				playSoundEntity(parent, 168, 128);
				spawnMagicEffectParticles(parent->x, parent->y, parent->z, 169);
			}
			else if ( my->skill[6] == SHADOW_SPELLCAST )
			{
				parent->shadowSpecialAbility(parent->monsterShadowInitialMimic);
				playSoundEntity(parent, 166, 128);
				spawnMagicEffectParticles(parent->x, parent->y, parent->z, my->skill[5]);
			}
			list_RemoveNode(my->mynode);
			return;
		}

		// calculate direction to caster and move.
		real_t tangent = atan2(parent->y - my->y, parent->x - my->x);
		real_t dist = sqrt(pow(my->x - parent->x, 2) + pow(my->y - parent->y, 2));
		real_t speed = dist / PARTICLE_LIFE;
		my->vel_x = speed * cos(tangent);
		my->vel_y = speed * sin(tangent);
		my->x += my->vel_x;
		my->y += my->vel_y;
	}
	else
	{
		// no parent, no target to travel to.
		list_RemoveNode(my->mynode);
		return;
	}

	if ( PARTICLE_LIFE < 0 )
	{
		list_RemoveNode(my->mynode);
		return;
	}
	else
	{
		--PARTICLE_LIFE;
	}
}

bool Entity::magicFallingCollision()
{
	hit.entity = nullptr;
	if ( z <= -5 || fabs(vel_z) < 0.01 )
	{
		// check if particle stopped or too high.
		return false;
	}

	if ( z >= 7.5 )
	{
		return true;
	}

	node_t* node;
	for ( node = map.entities->first; node != NULL; node = node->next )
	{
		Entity* entity = (Entity*)node->element;
		if ( entity == this )
		{
			continue;
		}
		if ( entityInsideEntity(this, entity) && !entity->flags[PASSABLE] )
		{
			hit.entity = entity;
			//hit.side = HORIZONTAL;
			return true;
		}
	}

	return false;
}
