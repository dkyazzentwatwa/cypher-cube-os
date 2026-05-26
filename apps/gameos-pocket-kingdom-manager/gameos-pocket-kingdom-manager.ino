#include <StoryGameLite.h>

using namespace WaveshareAmoledPorts;

const StoryGameProfile profile = {
  "Pocket Kingdom",
  "pocket_kingdom_manager",
  "Role: pocket sovereign, harvest planner, and reluctant diplomat.",
  "Balance workers, food, trade, and morale in a palm-sized kingdom.",
  {"First Crown", "Granary Season", "Border Lanterns", "Small Golden Age"},
  {"Keep the new crown from sliding off while the village watches.", "Fill the granary before weather, rats, or cousins become policy problems.", "Settle the border lantern dispute without marching anyone into mud.", "Build a golden age small enough to defend and kind enough to remember."},
  {"The crown is copper because gold made the first king unbearable.", "Granary mice are counted as citizens during hard winters.", "Border lanterns mark roads, graves, and who apologized last.", "A small golden age is measured in full bowls and quiet nights."},
  {"Morale", "Focus", "Food", "Unity", "Coin"},
  {"Copper Crown", "Granary Key", "Lantern Writ", "Harvest Bell"},
  {{"Build", "The masons raise a watchroom that doubles as a rain shelter.", -2, -4, -5, 7, -5, 18, 1}, {"Farm", "The fields answer with enough grain to quiet the council.", 3, -3, 12, 3, 0, 14, 2}, {"Trade", "The caravan overcharges until your scribe remembers old favors.", 0, -4, -3, 6, 7, 17, 4}, {"Decree", "Your decree is short, fair, and surprisingly hard to misquote.", -1, -3, 0, 10, 1, 20, 8}},
  0xFFE0
};

void setup() {
  beginStoryGame(profile);
}

void loop() {
  loopStoryGame();
}
