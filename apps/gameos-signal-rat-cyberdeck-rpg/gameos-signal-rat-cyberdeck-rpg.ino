#include <StoryGameLite.h>

using namespace WaveshareAmoledPorts;

const StoryGameProfile profile = {
  "Signal Rat",
  "signal_rat_cyberdeck_rpg",
  "Role: tunnel decker, packet thief, and bad-node survivor.",
  "Crawl signal tunnels, avoid bad nodes, and recover packets for hire.",
  {"Drain Node", "Blue Cable", "Rat King Cache", "Clean Exfil"},
  {"Enter the drain node and find the packet trail before the rain rises.", "Follow the blue cable through loops built to waste frightened minutes.", "Steal from the Rat King cache without waking every watcher on the mesh.", "Exfil clean and decide which employer deserves the dangerous truth."},
  {"Signal rats know every tunnel has two maps: water and data.", "Blue cable means official work, old money, or a trap with nice labels.", "The Rat King cache is many stolen packets tied by one ugly secret.", "Clean exfil is a myth, but clean enough still pays."},
  {"Cover", "Focus", "Charge", "Rep", "Cred"},
  {"Wet Map", "Blue Tap", "Cache Fang", "Exit Ghost"},
  {{"Sniff", "You catch a live route pulsing under the maintenance grate.", -2, -5, -2, 5, 1, 18, 1}, {"Crawl", "The tunnel narrows, but the packet trail stays bright.", -7, -4, -3, 7, 2, 20, 2}, {"Cache", "A hidden stash gives you charge, tape, and a name to avoid.", 4, 5, 8, 2, 1, 14, 4}, {"Exfil", "You ghost out before the lockout, boots full of water and proof.", -5, -5, -2, 10, 6, 22, 8}},
  0x7DFF
};

void setup() {
  beginStoryGame(profile);
}

void loop() {
  loopStoryGame();
}
