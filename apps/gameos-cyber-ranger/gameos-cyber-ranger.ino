#include <StoryGameLite.h>

using namespace WaveshareAmoledPorts;

const StoryGameProfile profile = {
  "Cyber Ranger",
  "cyber_ranger",
  "Role: mesh warden, incident scribe, and field network medic.",
  "Guard a small mesh outpost from probes, outages, and bad field intel.",
  {"Perimeter Ping", "Patch Night", "Relay Ghost", "Green Channel"},
  {"Sweep the outpost for unknown nodes before the supply convoy trusts the mesh.", "Keep services alive through a dirty firmware storm and a tired volunteer crew.", "Trace the relay that answers in your own packet timing.", "Publish a hardened route table and keep the valley talking."},
  {"The outpost mesh began as school roof antennas and borrowed batteries.", "Every patched node gets a green string tied to its mast for luck and inventory.", "Old relay boxes sometimes repeat messages from outages that happened years ago.", "The ranger oath is simple: no panic, no mystery port left open, no crew left offline."},
  {"Signal", "Focus", "Power", "Cred", "Parts"},
  {"Clean Map", "Patch Kit", "Spare Cell", "Relay Token"},
  {{"Scan", "The scan finds a soft relay pretending to be a weather station.", -2, -5, -2, 5, 1, 18, 1}, {"Patch", "You close the noisy service and write the fix where the next shift will see it.", 0, -6, -4, 7, -1, 20, 2}, {"Train", "The crew drills handoffs until the channel feels boring again.", 4, 6, -2, 6, 0, 14, 4}, {"Trace", "Three hops later, the ghost route points at a forgotten hilltop cache.", -5, -3, -3, 9, 3, 19, 8}},
  0x07E0
};

void setup() {
  beginStoryGame(profile);
}

void loop() {
  loopStoryGame();
}
