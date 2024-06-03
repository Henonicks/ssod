/************************************************************************************
 * 
 * The Seven Spells Of Destruction
 *
 * Copyright 1993,2001,2023 Craig Edwards <brain@ssod.org>
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
#include <ssod/game_util.h>
#include <fmt/format.h>
#include <ssod/database.h>
#include <dpp/dpp.h>
#include <ssod/aes.h>
#include <ssod/ssod.h>
#include <ssod/js.h>
#include <ssod/paragraph.h>
#include <ssod/game_player.h>

using namespace i18n;

dpp::component help_button(const dpp::interaction_create_t& event) {
	return dpp::component()
		.set_type(dpp::cot_button)
		.set_id(security::encrypt("player_nav_help"))
		.set_label(tr("GET_HELP", event))
		.set_url("https://discord.gg/brainbox")
		.set_style(dpp::cos_link);		
}

sale_info get_sale_info(const std::string& name) {
	auto res = db::query("SELECT * FROM game_item_descs WHERE name = ?", {name});
	if (res.empty()) {
		return sale_info{};
	}
	return sale_info{
		.flags = res[0].at("flags"),
		.value = atol(res[0].at("value")),
		.sellable = res[0].at("sellable") == "1",
		.quest_item = res[0].at("quest_item") == "1",
	};
}

std::string ellipsis(const std::string& in, size_t max_len) {
	if (in.length() > max_len) {
		return dpp::utility::utf8substr(in, 0, max_len) + "…";
	}
	return in;
}

std::string describe_item(const std::string& modifier_flags, const std::string& name, const dpp::interaction_create_t& event, bool ansi, size_t max_desc_len) {
	auto res = db::query("SELECT idesc FROM game_item_descs WHERE name = ?", {name});
	auto i = tr(item{ .name = name, .flags = modifier_flags }, res.size() ? res[0].at("idesc") : name, event);
	std::string rv{ellipsis(i.description, max_desc_len)};


	if (modifier_flags.substr(0, 3) == "ST+") {
		return fmt::format(fmt::runtime(ansi ? "\033[2;36m" + tr("STAMINA", event) + "\033[0m \033[2;34m+{}\033[0m: {}" :  tr("STAMINA", event) + " **+{}**: {}"), modifier_flags.substr(3), rv);
	} else if (modifier_flags.substr(0, 3) == "SK+") {
		return fmt::format(fmt::runtime(ansi ? "\033[2;36m" + tr("SKILL", event) + "\033[0m \033[2;34m+{}\033[0m: {}" : tr("SKILL", event) + " **+{}**: {}"), modifier_flags.substr(3), rv);
	} else if (modifier_flags.substr(0, 3) == "LK+") {
		return fmt::format(fmt::runtime(ansi ? "\033[2;36m" + tr("LUCK", event) + "\033[0m \033[2;34m+{}\033[0m: {}" : tr("LUCK", event) + " **+{}**: {}"), modifier_flags.substr(3), rv);
	} else if (modifier_flags.substr(0, 3) == "SN+") {
		return fmt::format(fmt::runtime(ansi ? "\033[2;36m" + tr("SNEAK", event) + "\033[0m \033[2;34m+{}\033[0m: {}" : tr("SNEAK", event) + " **+{}**: {}"), modifier_flags.substr(3), rv);
	} else if (modifier_flags.substr(0, 2) == "W+") {
		return fmt::format(fmt::runtime(ansi ? "\033[2;36m" + tr("WEAPON", event) + "\033[0m \033[2;34m+{}\033[0m: {}" : tr("WEAPON", event) + " **+{}**: {}"), modifier_flags.substr(2), rv);
	} else if (modifier_flags.substr(0, 2) == "A+") {
		return fmt::format(fmt::runtime(ansi ? "\033[2;36m" + tr("ARMOUR", event) + "\033[0m \033[2;34m+{}\033[0m: {}" : tr("ARMOUR", event) + " **+{}**: {}"), modifier_flags.substr(2), rv);
	} else if (!modifier_flags.empty() && modifier_flags[0] == 'W') {
		return fmt::format(fmt::runtime(ansi ? "\033[2;36m" + tr("WEAPON", event) + "\033[0m \033[2;34m{}\033[0m: {}" : tr("WEAPON", event) + " **{}**: {}"),modifier_flags.substr(1), rv);
	} else if (!modifier_flags.empty() && modifier_flags[0] == 'A') {
		return fmt::format(fmt::runtime(ansi ? "\033[2;36m" + tr("ARMOUR", event) + "\033[0m \033[2;34m{}\033[0m: {}" : tr("ARMOUR", event) + " **{}**: {}"),modifier_flags.substr(1), rv);
	} else {
		auto effect = db::query("SELECT * FROM passive_effect_types WHERE type = 'Consumable' AND requirements = ?", {name});
		if (!effect.empty()) {
			return fmt::format(fmt::runtime(ansi ? "\033[2;36m" + tr("CONSUMABLE", event) + "\033[0m \033[2;34m{}\033[0m: {}" : tr("CONSUMABLE", event) + ": {}"), rv);
		}
	}
	return rv;
}

void premium_required(const dpp::interaction_create_t& event) {
	event.reply(
		dpp::message(tr("PREMIUMUPSELL", event))
		.set_flags(dpp::m_ephemeral)
		.add_component(
			dpp::component().add_component(
				dpp::component()
				.set_type(dpp::cot_button)
				.set_id(security::encrypt("player_premium_cta"))
				.set_label(tr("GETPREMIUM", event))
				.set_url("https://premium.ssod.org/?user=" + event.command.usr.id.str())
				.set_style(dpp::cos_link)
			)
		)
	);
}

void trigger_effect(dpp::cluster& bot, const dpp::interaction_create_t& event, player& player, const std::string& type, const std::string& requirements) {
	uint64_t effect_id{0};
	auto eff_type = db::query("SELECT * FROM passive_effect_types WHERE type = ? AND requirements = ?", { type, requirements });
	if (type.empty()) {
		return;
	}
	effect_id = atol(eff_type[0].at("id"));
	auto status = db::query("SELECT * FROM passive_effect_status WHERE user_id = ? AND passive_effect_id = ?", { event.command.usr.id, effect_id });
	if (!status.empty()) {
		return;
	}
	db::query(
		"INSERT INTO passive_effect_status (user_id, passive_effect_id, current_state, last_transition_time) VALUES(?, ?, 'active', UNIX_TIMESTAMP())",
		{ event.command.usr.id, effect_id }
	);
	bot.log(dpp::ll_debug, "Passive effect " + eff_type[0].at("type") + "/" + eff_type[0].at("requirements") + " on player " + event.command.usr.id.str() + " started");
	paragraph p;
	p.cur_player = &player;
	p.id = player.paragraph;
	js::run(eff_type[0].at("on_start"), p, player, {});
}

void check_effects(dpp::cluster& bot) {
	auto rs = db::query("SELECT * FROM `passive_effect_status` join passive_effect_types on passive_effect_id = passive_effect_types.id where UNIX_TIMESTAMP() > last_transition_time + IF(current_state = 'active', duration, withdrawl)");
	for (const auto& row : rs) {
		dpp::interaction_create_t event;
		event.command.usr.id = atoll(row.at("user_id"));
		player player = get_live_player(event);
		/* Initialise this paragraph 'empty' as we don't want to parse it and trigger any changes within */
		paragraph p;
		p.cur_player = &player;
		p.id = player.paragraph;
		if (row.at("current_state") == "active") {
			db::query("UPDATE passive_effect_status SET current_state = 'withdrawl' WHERE id = ?", {row.at("id")});
			bot.log(dpp::ll_debug, "Passive effect " + row.at("type") + "/" + row.at("requirements") + " on player " + event.command.usr.id.str() + " moved to state 'withdrawl'");
			js::run(row.at("on_end"), p, player, {});
		} else if (row.at("current_state") == "withdrawl") {
			js::run(row.at("on_after"), p, player, {});
			db::query("DELETE FROM passive_effect_status WHERE id = ?", {row.at("id")});
			bot.log(dpp::ll_debug, "Passive effect " + row.at("type") + "/" + row.at("requirements") + " on player " + event.command.usr.id.str() + " ended");
		}

	}
}

std::string human_readable_spell_name(const std::string& spell, const dpp::interaction_create_t& event) {
	return tr(dpp::uppercase(spell), event);
}

std::string human_readable_herb_name(const std::string& herb, const dpp::interaction_create_t& event) {
	return tr(dpp::uppercase(herb), event);
}

spell_info get_spell_info(const std::string& name) {
	auto rs = db::query("SELECT * FROM spells WHERE name = ?", {name});
	if (rs.empty()) {
		return {};
	}
	return { .name = rs[0].at("name"), .component_herb = rs[0].at("herb"), .combat_rating = atol(rs[0].at("combat_rating")), .mana_cost = atol(rs[0].at("mana_cost")) };
}
