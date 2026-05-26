#include <StoryGameLite.h>

using namespace WaveshareAmoledPorts;

const StoryGameProfile profile = {
  "Dungeon Courier",
  "dungeon_courier",
  "Role: oathbound courier, trap reader, and reluctant hero.",
  "Deliver sealed parcels through trap rooms, guild halls, and monster tolls.",
  {"Copper Gate", "Mimic Hall", "Underking Toll", "Last Door"},
  {"Carry a sealed writ past the Copper Gate before the wax sigil sweats loose.", "Survive the hall where furniture asks questions and teeth answer first.", "Cross the Underking's toll bridge without spending the kingdom's secret.", "Reach the Last Door and decide whether a courier's oath covers prophecy."},
  {"Courier guild law says a package is not late until the bearer is dead twice.", "Mimics learned etiquette from nobles and hunger from everyone else.", "The Underking taxes footsteps, lies, and names spoken with fear.", "The Last Door opens inward because heroes are expected to push too hard."},
  {"Grit", "Wits", "Rations", "Renown", "Coin"},
  {"Wax Writ", "Silver Chalk", "Toll Token", "Door Name"},
  {{"Route", "You spot murder holes above the pretty tiles and take the ugly stairs.", -3, -5, -2, 5, 0, 18, 1}, {"Barter", "A bored ogre accepts soup, gossip, and one copper less than tradition.", -1, -4, -6, 7, -2, 16, 4}, {"Rest", "You camp behind a saint statue and wake before the spiders vote.", 10, 8, -3, 1, 0, 12, 2}, {"Sprint", "You run the blade corridor on instinct and arrive breathless but early.", -9, -6, -2, 9, 4, 22, 8}},
  0xFBE0
};

void setup() {
  beginStoryGame(profile);
}

void loop() {
  loopStoryGame();
}
