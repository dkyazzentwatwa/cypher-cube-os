#include <StoryGameLite.h>

using namespace WaveshareAmoledPorts;

const StoryGameProfile profile = {
  "Star Trail",
  "star_trail_rancher",
  "Role: convoy rancher, starherd guide, and campfire mechanic.",
  "Drive a cosmic ranch convoy through weather, markets, and odd signals.",
  {"Launch Pasture", "Comet Ford", "Market Nebula", "Long Camp"},
  {"Launch the convoy before the youngest starcalves chase the beacon.", "Ford the comet stream while ice sings against the hulls.", "Sell goods in the nebula market without trading away your route.", "Make long camp where the herd can finally glow without fear."},
  {"Starcalves follow songs, engine hum, and anyone carrying sweet mineral salt.", "Comet fords change course when pilots brag.", "Nebula markets price goods by rarity, smell, and drama.", "Long camp is where every convoy story becomes a map for someone smaller."},
  {"Herd", "Focus", "Stores", "Trust", "Scrip"},
  {"Salt Bell", "Comet Rope", "Market Pin", "Camp Star"},
  {{"Navigate", "You bend the convoy around a gravity bruise before it becomes news.", -3, -5, -3, 5, 1, 18, 1}, {"Tend", "Starcalves settle as you clean frost from their glowing hides.", 8, 6, -3, 7, 0, 15, 2}, {"Trade", "The market takes wool, gives parts, and pretends that was fair.", 0, -3, -4, 5, 6, 17, 4}, {"Camp", "Repairs hold under violet sky while the herd dreams in sparks.", 9, 7, -5, 3, -1, 13, 8}},
  0xB7E0
};

void setup() {
  beginStoryGame(profile);
}

void loop() {
  loopStoryGame();
}
