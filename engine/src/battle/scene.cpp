#include <algorithm>

#include "battle/battle.hpp"

namespace ff6 {

// A place of a scene: the character is found by its actor among the ones that the game keeps; one that is not there takes no place.
bool Battle::readSceneSlot(const uint8_t* slot, SceneSlot& out) const {
    if (slot[0] == FF6_CHAR_AI_NONE) return false;
    const int actor = slot[0] & 0x3F;
    out = SceneSlot{};
    for (int c = 0; c < FF6_CHAR_COUNT && out.character < 0; ++c) {
        if (party_.characters[static_cast<size_t>(c)].actor == actor) out.character = c;
    }
    if (out.character < 0) return false;
    out.script = slot[2] != FF6_CHAR_AI_NONE ? 0x100 | slot[2] : -1;
    out.gfx = slot[1] != FF6_CHAR_AI_NONE ? slot[1] : -1;
    out.x = slot[3] != FF6_CHAR_AI_NONE ? slot[3] : -1;
    out.y = slot[4] != FF6_CHAR_AI_NONE ? slot[4] : -1;
    out.enemy = (slot[0] & FF6_CHAR_AI_ENEMY) != 0;
    out.offstage = (slot[0] & FF6_CHAR_AI_NOT_IN_PARTY) != 0;
    return true;
}

// The scene of a battle, from the table of the character AI (InitParty): characters of the story take places among the fighters
// or among the monsters, hide the party, or wait out of the battle. A character is found by its actor among the ones that the
// game keeps; the one that is not there takes no place.
void Battle::applyCharAi(const BattleFormation& formation, std::vector<int>& fighters) {
    sceneSlots_.assign(fighters.size(), SceneSlot{});
    for (size_t i = 0; i < fighters.size(); ++i) sceneSlots_[i].character = fighters[i];
    if (!(formation.flags & FF6_BATTLE_FLAG_CHAR_AI)) return;
    const int index = formation.aux & 0xFF;
    if (index <= 0 || index >= FF6_CHAR_AI_COUNT) return;
    const uint8_t* entry = &data_.charAi[static_cast<size_t>(index) * FF6_CHAR_AI_BYTES];
    hideNames_ = (entry[0] & FF6_CHAR_AI_HIDE_NAMES) != 0;
    sceneBackground_ = entry[1] != FF6_CHAR_AI_NONE ? entry[1] : -1;
    sceneSong_ = entry[3] != FF6_CHAR_AI_NONE ? entry[3] : -1;
    targetMask_ = entry[2];  // the monsters that can be aimed at, a bit for each place
    if (entry[0] & FF6_CHAR_AI_HIDE_PARTY) {
        fighters.clear();
        sceneSlots_.clear();
    }
    for (int k = 0; k < 4; ++k) {
        SceneSlot placed;
        if (!readSceneSlot(entry + 4 + k * 5, placed)) continue;
        const int character = placed.character;
        const auto among = std::find(fighters.begin(), fighters.end(), character);
        if (among != fighters.end()) {  // a member of the party that the scene gives a script, a sprite or a side
            const size_t at = static_cast<size_t>(among - fighters.begin());
            if (placed.enemy) {
                fighters.erase(among);
                sceneSlots_.erase(sceneSlots_.begin() + static_cast<std::ptrdiff_t>(at));
                sceneEnemies_.push_back(placed);
            } else {
                sceneSlots_[at] = placed;
            }
        } else if (placed.enemy) {
            sceneEnemies_.push_back(placed);
        } else if (fighters.size() < static_cast<size_t>(FF6_BATTLE_PARTY_SLOTS)) {
            fighters.push_back(character);
            sceneSlots_.push_back(placed);
        }
    }
}

// What the scene gives to the character of a place of the party.
void Battle::applyScenePlace(Combatant& c, size_t slot) {
    if (slot >= sceneSlots_.size()) return;
    const SceneSlot& scene = sceneSlots_[slot];
    c.gfx = scene.gfx;
    c.placeX = scene.x;
    c.placeY = scene.y;
    c.offstage = scene.offstage;
    c.aiId = scene.script;
    if (scene.script >= 0) initAi(static_cast<int>(slot));
}

// A character that fights for the monsters stands among them: a monster by its script, with the stats of the character when the game
// keeps some.
Combatant* Battle::placeEnemyChar(const SceneSlot& enemy) {
    if (enemy.script < 0) return nullptr;
    size_t at = FF6_BATTLE_COMBATANTS;
    for (size_t i = FF6_BATTLE_COMBATANTS; i-- > FF6_BATTLE_PARTY_SLOTS;) {  // from the last place, which a formation rarely fills
        if (!combatants_[i].present) {
            at = i;
            break;
        }
    }
    if (at == FF6_BATTLE_COMBATANTS) return nullptr;
    loadMonster(at, enemy.script);
    Combatant& c = combatants_[at];
    if (party_.characters[static_cast<size_t>(enemy.character)].maxHp > 0) {
        fillCharacter(c, enemy.character);
        if (c.hp <= 0) c.hp = c.maxHp;
    }
    c.charIndex = enemy.character;
    c.gfx = enemy.gfx >= 0 ? enemy.gfx : characterSprite(enemy.character);
    c.placeX = enemy.x;
    c.placeY = enemy.y;
    c.offstage = enemy.offstage;
    refreshPace(c);
    return &c;
}

void Battle::placeEnemyChars() {
    for (const SceneSlot& enemy : sceneEnemies_) placeEnemyChar(enemy);
}

// A character of the party that a scene brings in or takes out of the battle.
void Battle::showCharacter(int actor, bool shown) {
    for (int i = 0; i < FF6_BATTLE_PARTY_SLOTS; ++i) {
        Combatant& c = combatants_[static_cast<size_t>(i)];
        if (c.present && !c.monster && party_.characters[static_cast<size_t>(c.id)].actor == actor) c.offstage = !shown;
    }
}

// The scenes that change who is in the battle (add_char_target and remove_char_target of the battle events): Sabin takes the place of
// the party that came, and Relm joins in.
void Battle::sceneEffects(int event) {
    if (event == FF6_BATTLE_EVENT_SABIN_INTRO) {
        showCharacter(FF6_ACTOR_SABIN, true);
        for (const int actor : {0, 1, 4}) showCharacter(actor, false);  // Terra, Locke and Edgar
    } else if (event == FF6_BATTLE_EVENT_RELM_ULTROS) {
        showCharacter(8, true);
    }
}

void Battle::triggerScene(int event) {
    BattleEvent scene;
    scene.kind = BattleEvent::Kind::Scene;
    scene.damage = event;
    events_.push_back(scene);
    sceneEffects(event);
}

}  // namespace ff6
