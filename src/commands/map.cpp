/************************************************************************************
 * 
 * The Seven Spells Of Destruction
 *
 * Copyright 1993,2001,2023 Craig Edwards <support@sporks.gg>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ************************************************************************************/
#include <ssod/ssod.h>
#include <ssod/database.h>
#include <ssod/commands/map.h>
#include <ssod/database.h>
#include <ssod/sentry.h>
#include <ssod/game_date.h>

dpp::slashcommand map_command::register_command(dpp::cluster& bot)
{
	return dpp::slashcommand("map", "Show a map of the game world", bot.me.id);
}

void map_command::route(const dpp::slashcommand_t &event)
{
	dpp::cluster* bot = event.from->creator;
	event.reply("Not implemented");
}
