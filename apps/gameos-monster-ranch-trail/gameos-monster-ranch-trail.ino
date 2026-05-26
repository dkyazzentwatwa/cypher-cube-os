#include <StoryGameLite.h>

using namespace WaveshareAmoledPorts;

const StoryGameProfile profile = {
  "Monster Ranch",
  "monster_ranch_trail",
  "Role: trail boss, monster keeper, and storm reader.",
  "Guide odd little beasts across ranch land without losing feed or trust.",
  {"Barn Gate", "Glass Creek", "Thunder Flats", "Home Pasture"},
  {"Move the herd out before the smallest monsters learn how gates work.", "Cross Glass Creek while the water reflects animals you do not own yet.", "Keep the herd together through thunder that speaks in old brands.", "Reach home pasture with more trust than bite marks."},
  {"Monster ranchers name every beast twice: once for manners and once for magic.", "Glass Creek shows hungry futures unless someone sings over the crossing.", "Thunder Flats were branded by sky giants who lost their cattle.", "A home pasture is any fence the herd chooses not to test."},
  {"Bond", "Focus", "Feed", "Trust", "Scrip"},
  {"Blue Halter", "Creek Song", "Storm Bell", "Home Brand"},
  {{"Feed", "Warm mash settles the nippers before they chew the moon again.", 5, 2, -8, 6, -1, 13, 1}, {"Scout", "You find a dry ridge and only one suspiciously friendly burrow.", -3, -5, -2, 5, 0, 18, 2}, {"Groom", "Mud, burrs, and bad temper come off in careful handfuls.", 7, 6, -2, 7, 0, 15, 4}, {"Herd", "The herd surges as one bright, weird river of horns and paws.", -7, -5, -4, 9, 4, 22, 8}},
  0x87F0
};

void setup() {
  beginStoryGame(profile);
}

void loop() {
  loopStoryGame();
}
