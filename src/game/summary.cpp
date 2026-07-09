#include "game/summary.h"

#include <cstdio>

namespace toy {

MatchSummary buildSummary(const Match& match)
{
	MatchSummary s;
	s.turns = match.turnNumber;
	s.winner = match.winner;

	for (const MatchEvent& e : match.log) {
		switch (e.type) {
		case MatchEvent::Type::Damage:
			if (e.actor >= 0 && e.actor < kMaxPlayers) {
				s.damageDealt[static_cast<size_t>(e.actor)] += e.value;
			}
			if (e.target >= 0 && e.target < kMaxPlayers) {
				s.damageTaken[static_cast<size_t>(e.target)] += e.value;
			}
			break;
		case MatchEvent::Type::CardPlayed:
			if (e.actor >= 0 && e.actor < kMaxPlayers) {
				s.cardsPlayed[static_cast<size_t>(e.actor)] += 1;
			}
			break;
		case MatchEvent::Type::WorldEvent:
			++s.worldEvents;
			break;
		case MatchEvent::Type::Info:
			// Heal lines use Info with value > 0 from Medkit / rations approximate via text skip.
			break;
		default:
			break;
		}
	}
	return s;
}

void formatSummaryLine(const MatchSummary& s, const Match& match, char* out, int outCap)
{
	if (!out || outCap <= 0) {
		return;
	}
	if (s.winner >= 0 && s.winner < match.config.playerCount) {
		std::snprintf(out, outCap, "%s wins in %d turns · %d world events",
					  match.players[static_cast<size_t>(s.winner)].name, s.turns, s.worldEvents);
	} else {
		std::snprintf(out, outCap, "Draw after %d turns · %d world events", s.turns, s.worldEvents);
	}
}

} // namespace toy
