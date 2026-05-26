#include <StoryGameLite.h>

using namespace WaveshareAmoledPorts;

const StoryGameProfile profile = {
  "Tiny Wasteland",
  "tiny_wasteland",
  "Role: crew lead, water counter, and scrap-route prophet.",
  "Scavenge, shelter, and keep a small crew alive across harsh ground.",
  {"Dust Mile", "Glass Town", "Red Storm", "Green Rumor"},
  {"Cross the dust mile with enough water to still make choices.", "Scavenge Glass Town before sunset turns the windows into knives.", "Shelter through the red storm and keep the crew from splitting.", "Find the green rumor and decide whether hope is a place or a trap."},
  {"Dust mile markers are old solar posts with names scratched over names.", "Glass Town shines because every wall remembers heat.", "Red storms carry voices from radios with no batteries.", "The green rumor has moved for ten years and still everyone walks toward it."},
  {"Grit", "Focus", "Water", "Hope", "Scrap"},
  {"Water Map", "Glass Knife", "Storm Tarp", "Green Seed"},
  {{"Scavenge", "You pry scrap from a bus that still displays route 404.", -4, -5, -3, 4, 6, 18, 1}, {"Scout", "A ridge line shows shelter, smoke, and one bad shortcut.", -3, -5, -2, 5, 1, 17, 2}, {"Shelter", "Canvas snaps, grit hisses, and nobody leaves the circle.", 10, 7, -5, 4, -1, 13, 4}, {"Trade", "The trader swaps fair enough after seeing your crew still laughs.", -1, -3, 3, 7, 3, 16, 8}},
  0xFD20
};

void setup() {
  beginStoryGame(profile);
}

void loop() {
  loopStoryGame();
}
