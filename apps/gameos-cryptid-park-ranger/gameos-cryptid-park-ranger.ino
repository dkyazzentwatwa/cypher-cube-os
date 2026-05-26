#include <StoryGameLite.h>

using namespace WaveshareAmoledPorts;

const StoryGameProfile profile = {
  "Cryptid Ranger",
  "cryptid_park_ranger",
  "Role: junior ranger, field folklorist, and night patrol lead.",
  "Patrol a strange park, log sightings, manage gear, and keep visitors calm.",
  {"Misty Trailhead", "Old Fire Tower", "Moonlit Sinkhole", "The Quiet Preserve"},
  {"Find what keeps circling the trail cameras before the weekend hikers arrive.", "Reach the tower, compare claw marks, and decide whether the warning signs are enough.", "Map the sinkhole tunnels while the radio chatter turns into copied voices.", "Prove the preserve can stay open without feeding the thing under the pines."},
  {"Rangers call the first prints deer-lies: hoof shapes that end in too many toes.", "The fire tower logbook mentions green lamps moving between trees in 1979.", "Old limestone caves run beneath the picnic loop and make every sound arrive twice.", "The preserve survives by bargains: honest reports, closed paths, and steady nerves."},
  {"Nerve", "Focus", "Gear", "Trust", "Budget"},
  {"Track Cast", "Tower Key", "Bait Tin", "Quiet Map"},
  {{"Patrol", "Fresh prints cross your beam. You mark distance, depth, and direction.", -5, -4, -3, 6, 0, 18, 1}, {"Interview", "A shaken camper repeats one detail: the antlers were listening.", 0, -3, 0, 8, 1, 16, 0}, {"Camp", "You fix straps, brew coffee, and stop the team from inventing rumors.", 9, 8, 5, 2, -1, 12, 4}, {"Report", "Your clean field note earns a permit extension and a late-night warning.", 0, 2, -2, 10, 2, 15, 8}},
  0x07FF
};

void setup() {
  beginStoryGame(profile);
}

void loop() {
  loopStoryGame();
}
