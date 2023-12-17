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
#include <set>
#include <fmt/format.h>
#include <ssod/listeners.h>
#include <ssod/database.h>
#include <ssod/ssod.h>
#include <ssod/command.h>
#include <ssod/sentry.h>

#include <ssod/commands/info.h>
#include <ssod/commands/start.h>
#include <ssod/commands/map.h>
#include <ssod/commands/admin.h>
#include <ssod/commands/lore.h>

#include <ssod/botlist.h>
#include <ssod/botlists/topgg.h>
#include <ssod/botlists/discordbotlist.h>
#include <ssod/botlists/infinitybots.h>

namespace listeners {

	/**
	 * @brief Welcome a new guild to the bot with some advice on getting started
	 * 
	 * @param bot 
	 * @param guild_id 
	 * @param channel_id 
	 */
	void send_welcome(dpp::cluster& bot, dpp::snowflake guild_id, dpp::snowflake channel_id) {
		bot.message_create(
			dpp::message(channel_id, "")
			.add_embed(
				dpp::embed()
				.set_description("Detailed welcome message will go here in due course")
				.set_title("Welcome, Adventurers!")
				.set_color(0xd5b994)
				.set_url("https://ssod.org/")
				.set_thumbnail(bot.me.get_avatar_url())
				.set_footer("The Seven Spells Of Destruction", bot.me.get_avatar_url())
			)
		);
		/* Probably successfully welcomed */
		db::query("UPDATE guild_cache SET welcome_sent = 1 WHERE id = ?", {guild_id});
	}

	/**
	 * @brief Check every 30 seconds for new guilds and welcome them
	 * 
	 * @param bot cluster ref
	 */
	void welcome_new_guilds(dpp::cluster& bot) {
		auto result = db::query("SELECT id FROM guild_cache WHERE welcome_sent = 0");
		for (const auto& row : result) {
			/* Determine the correct channel to send to */
			dpp::snowflake guild_id = row.at("id");
			bot.log(dpp::ll_info, "New guild: " + guild_id.str());
			bot.guild_get(guild_id, [&bot, guild_id](const auto& cc) {
				if (cc.is_error()) {
					/* Couldn't fetch the guild - kicked within 30 secs of inviting, bummer. */
					db::query("UPDATE guild_cache SET welcome_sent = 1 WHERE id = ?", {guild_id});
					return;
				}
				dpp::guild guild = std::get<dpp::guild>(cc.value);
				/* First try to send the message to system channel or safety alerts channel if defined */
				if (!guild.system_channel_id.empty()) {
					send_welcome(bot, guild.id, guild.system_channel_id);
					return;
				}
				if (!guild.safety_alerts_channel_id.empty()) {
					send_welcome(bot, guild.id, guild.safety_alerts_channel_id);
					return;
				}
				/* As a last resort if they dont have one of those channels set up, find a named
				 * text channel that looks like its the main general/chat channel
				 */
				bot.channels_get(guild_id, [&bot, guild_id, guild](const auto& cc) {
					if (cc.is_error()) {
						/* Couldn't fetch the channels - kicked within 30 secs of inviting, bummer. */
						db::query("UPDATE guild_cache SET welcome_sent = 1 WHERE id = ?", {guild_id});
						return;
					}
					dpp::channel_map channels = std::get<dpp::channel_map>(cc.value);
					dpp::snowflake selected_channel_id, first_text_channel_id;
					for (const auto& c : channels) {
						const dpp::channel& channel = c.second;
						std::string lowername = dpp::lowercase(channel.name);
						if ((lowername == "general" || lowername == "chat" || lowername == "moderators") && channel.is_text_channel()) {
							selected_channel_id = channel.id;
							break;
						} else if (channel.is_text_channel()) {
							first_text_channel_id = channel.id;
						}
					}
					if (selected_channel_id.empty() && !first_text_channel_id.empty()) {
						selected_channel_id = first_text_channel_id;
					}
					if (!selected_channel_id.empty()) {
						send_welcome(bot, guild_id, selected_channel_id);
					} else {
						/* What sort of server has NO text channels and invites a game bot??? */
						db::query("UPDATE guild_cache SET welcome_sent = 1 WHERE id = ?", {guild_id});
					}
				});
			});
		}
	}

	void on_ready(const dpp::ready_t &event) {
		dpp::cluster& bot = *event.from->creator;
		if (dpp::run_once<struct register_bot_commands>()) {
			bot.global_bulk_command_create({
				register_command<info_command>(bot),
				register_command<start_command>(bot),
				register_command<map_command>(bot),
				register_command<lore_command>(bot),
			});
			bot.guild_bulk_command_create({
				register_command<admin_command>(bot),
			}, 537746810471448576);

			auto set_presence = [&bot]() {
				bot.current_application_get([&bot](const dpp::confirmation_callback_t& v) {
					if (!v.is_error()) {
						dpp::application app = std::get<dpp::application>(v.value);
						bot.set_presence(dpp::presence(dpp::ps_online, dpp::at_game, fmt::format("on {} servers", app.approximate_guild_count)));
					}
				});
			};

			bot.start_timer([&bot, set_presence](dpp::timer t) {
				set_presence();
			}, 240);
			bot.start_timer([&bot](dpp::timer t) {
				post_botlists(bot);
			}, 60 * 15);
			bot.start_timer([&bot](dpp::timer t) {
				welcome_new_guilds(bot);
			}, 30);

			set_presence();
			welcome_new_guilds(bot);

			register_botlist<topgg>();
			register_botlist<discordbotlist>();
			register_botlist<infinitybots>();

			post_botlists(bot);
		}
	}

	void on_guild_create(const dpp::guild_create_t &event) {
		if (event.created->is_unavailable()) {
			return;
		}
		db::query("INSERT INTO guild_cache (id, owner_id, name, user_count) VALUES(?,?,?,?) ON DUPLICATE KEY UPDATE owner_id = ?, name = ?, user_count = ?", { event.created->id, event.created->owner_id, event.created->name, event.created->member_count, event.created->owner_id, event.created->name, event.created->member_count });
	}

	void on_guild_delete(const dpp::guild_delete_t &event) {
		if (!event.deleted.is_unavailable()) {
			db::query("DELETE FROM guild_cache WHERE id = ?", { event.deleted.id });
			event.from->creator->log(dpp::ll_info, "Removed from guild: " + event.deleted.id.str());
		}
	}

	void on_slashcommand(const dpp::slashcommand_t &event) {
		event.from->creator->log(
			dpp::ll_info,
			fmt::format(
				"COMMAND: {} by {} ({} Guild: {})",
				event.command.get_command_name(),
				event.command.usr.format_username(),
				event.command.usr.id,
				event.command.guild_id
			)
		);
		route_command(event);
	}

	void reply_to_ping(const dpp::message_create_t &ev) {
		std::vector<std::string> replies{
			"Hail, @user!",
			"Yes, @user?",
			"@user, You probably want the /start command",
		};
		std::vector<std::string>::iterator rand_iter = replies.begin();
		std::advance(rand_iter, std::rand() % replies.size());
		std::string response = replace_string(*rand_iter, "@user", ev.msg.author.get_mention());
		ev.from->creator->message_create(
			dpp::message(ev.msg.channel_id, response)
				.set_allowed_mentions(true, false, false, true, {}, {})
				.set_reference(ev.msg.id, ev.msg.guild_id, ev.msg.channel_id, false)
		);
	}

	void on_message_create(const dpp::message_create_t &event) {
		auto guild_member = event.msg.member;

		/* If the author is a bot or webhook, stop the event (no checking). */
		if (event.msg.author.is_bot() || event.msg.author.id.empty()) {
			return;
		}

		/* Check if we are mentioned in the message, if so send a sarcastic reply */
		for (const auto& ping : event.msg.mentions) {
			if (ping.first.id == event.from->creator->me.id) {
				reply_to_ping(event);
				/* Don't return, just break, because people might still try
				 * to put dodgy images in the message too
				 */
				break;
			}
		}

	}
}
