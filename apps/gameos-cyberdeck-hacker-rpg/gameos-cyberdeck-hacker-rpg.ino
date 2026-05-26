#include <StoryGameLite.h>

using namespace WaveshareAmoledPorts;

const StoryGameProfile profile = {
  "Cyberdeck RPG",
  "cyberdeck_hacker_rpg",
  "Role: careful deckrunner, favor broker, and heat manager.",
  "Run careful ops from a pocket deck, balancing heat, access, and reputation.",
  {"Coffee Shop Recon", "Backdoor Hymnal", "Black Ice Wake", "Dead Drop Dawn"},
  {"Build a target map without letting the cafe camera learn your face.", "Open the hymn-coded backdoor and decide which favor to burn.", "Cross black ice with the client screaming for proof.", "Deliver the archive, wipe the route, and choose who gets paid."},
  {"Good deckrunners keep three lies ready: job, name, and reason for leaving.", "The Hymnal is an old access pattern hidden in corporate training audio.", "Black ice does not chase. It waits until you need to hurry.", "Every dead drop has a witness. The trick is making the witness owe you."},
  {"Cover", "Focus", "Tools", "Rep", "Cred"},
  {"Clean Proxy", "Root Note", "Ice Charm", "Drop Key"},
  {{"Recon", "You profile guards, cameras, and coffee refills until the route appears.", -2, -5, -1, 5, 0, 18, 1}, {"Exploit", "The exploit lands hard, opening access while the heat clock wakes up.", -7, -7, -4, 9, 4, 23, 2}, {"Hide", "You salt logs, ditch a burner, and let the trail cool.", 5, 7, -3, 2, -1, 13, 4}, {"Deliver", "The client accepts the packet, but the bonus arrives with a new problem.", -1, -2, -1, 8, 6, 17, 8}},
  0xFD20
};

void setup() {
  beginStoryGame(profile);
}

void loop() {
  loopStoryGame();
}
