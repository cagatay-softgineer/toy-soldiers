#include "game/play_reason.h"

#include "game/cards.h"
#include "game/events.h"
#include "game/rules.h"

namespace toy {

const char* describeIllegalPlay(const Match& match, int playerIndex, int handIndex, int targetPlayer)
{
	if (match.phase != Phase::Playing) {
		if (match.phase == Phase::Lobby) {
			return "Match has not started yet.";
		}
		if (match.phase == Phase::GameOver) {
			return "Match is over.";
		}
		return "Not in a playable phase.";
	}
	if (playerIndex != match.activePlayer) {
		return "Not your turn.";
	}
	if (playerIndex < 0 || playerIndex >= match.config.playerCount) {
		return "Invalid player.";
	}
	const Player& p = match.players[static_cast<size_t>(playerIndex)];
	if (p.eliminated) {
		return "You are eliminated.";
	}
	if (handIndex < 0 || handIndex >= static_cast<int>(p.hand.size())) {
		return "Empty hand or invalid card slot.";
	}
	const CardInstance& inst = p.hand[static_cast<size_t>(handIndex)];
	const CardDef* def = findCard(inst.defId);
	if (!def) {
		return "Unknown card.";
	}
	if (def->klass == CardClass::Attack && !(def->keywords & KwAOE)) {
		if (targetPlayer < 0 || targetPlayer >= match.config.playerCount) {
			return "Pick a valid target tower.";
		}
		if (targetPlayer == playerIndex) {
			return "Cannot attack yourself.";
		}
		if (match.players[static_cast<size_t>(targetPlayer)].eliminated) {
			return "Target is eliminated.";
		}
		if (match.players[static_cast<size_t>(targetPlayer)].control == SeatControl::Empty) {
			return "Target seat is empty.";
		}
		if (sameTeam(match, playerIndex, targetPlayer)) {
			return "Cannot target your teammate.";
		}
		if (eventForcesAdjacent(match) || !match.config.freeTargeting || (def->keywords & KwAdjacentOnly)) {
			const int diff = targetPlayer > playerIndex ? targetPlayer - playerIndex : playerIndex - targetPlayer;
			const int wrap = match.config.playerCount - 1;
			if (!(diff == 1 || diff == wrap)) {
				if (eventForcesAdjacent(match)) {
					return "Sandstorm: adjacent targets only.";
				}
				return "Target is out of range (adjacent only).";
			}
		}
	}
	// Fallback — canPlayCard is false for another reason
	if (!canPlayCard(match, playerIndex, handIndex, targetPlayer)) {
		return "Illegal play.";
	}
	return "";
}

} // namespace toy
