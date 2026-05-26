#include <StoryGameLite.h>

using namespace WaveshareAmoledPorts;

const StoryGameProfile profile = {
  "Pocket Detective",
  "pocket_detective_agency",
  "Role: street detective, note keeper, and tiny office legend.",
  "Work tiny cases with clues, witnesses, stakeouts, and careful notes.",
  {"Cold Desk", "Alley Ledger", "Neon Witness", "Case Closed"},
  {"Turn a cheap office and a stranger's envelope into your first real case.", "Follow the ledger through alleys that know more than they should.", "Protect the witness long enough for the lie to contradict itself.", "Close the case without letting the city file off the sharp parts."},
  {"The agency sign is painted on cardboard, but the coffee is serious.", "Every alley ledger has two totals: money owed and fear collected.", "Neon witnesses remember color better than faces.", "A closed case still knocks when the wrong person gets comfortable."},
  {"Grit", "Focus", "Leads", "Cred", "Cash"},
  {"Photo Clue", "Ledger Page", "Witness Pin", "Case Seal"},
  {{"Search", "A matchbook under the radiator names a club that burned down twice.", -2, -5, -2, 5, 0, 18, 1}, {"Question", "The witness edits a lie into something useful.", 0, -4, -1, 8, 1, 17, 2}, {"Stakeout", "Rain, bad coffee, and patience catch the handoff at 2:13.", -6, -6, -3, 9, 2, 22, 4}, {"Close", "You pin the story to the desk before it wriggles into politics.", -1, -3, -2, 10, 5, 19, 8}},
  0xFFFF
};

void setup() {
  beginStoryGame(profile);
}

void loop() {
  loopStoryGame();
}
