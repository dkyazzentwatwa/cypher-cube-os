#include <StoryGameLite.h>

using namespace WaveshareAmoledPorts;

const StoryGameProfile profile = {
  "Star Trader",
  "star_trader_pocket_frontier",
  "Role: frontier captain, market reader, and hull patcher.",
  "Haul cargo, read prices, dodge risk, and keep the little ship flying.",
  {"Dock Seven", "Salted Lane", "Pirate Moon", "Frontier Ledger"},
  {"Buy cheap cargo before Dock Seven fees eat the launch window.", "Cross the salted lane where sensors lie and engines cough dust.", "Slip Pirate Moon with your hold intact and your transponder boring.", "Balance the frontier ledger and become welcome at the next port."},
  {"Dock Seven sells fuel, rumors, and noodles in that order.", "The salted lane was mined, cleared, cursed, and then mined again.", "Pirate Moon broadcasts recipes when it wants ships to relax.", "A frontier ledger records profit, favors, and who got home."},
  {"Hull", "Focus", "Fuel", "Name", "Cred"},
  {"Cargo Seal", "Lane Chart", "Moon Code", "Trade Writ"},
  {{"Buy", "You buy underpriced filters from a broker too sleepy to lie well.", 0, -3, -2, 2, -5, 14, 1}, {"Haul", "The lane bucks hard, but the cargo net holds.", -6, -5, -7, 6, 4, 20, 2}, {"Repair", "A patch plate and three ugly welds keep vacuum outside.", 10, 5, -4, 1, -3, 13, 4}, {"Sell", "The sale clears debt and earns a berth nobody spits near.", -1, -3, -2, 10, 8, 21, 8}},
  0x7BEF
};

void setup() {
  beginStoryGame(profile);
}

void loop() {
  loopStoryGame();
}
