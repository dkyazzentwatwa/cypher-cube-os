#include <StoryGameLite.h>

using namespace WaveshareAmoledPorts;

const StoryGameProfile profile = {
  "Haunted Radio",
  "haunted_radio_operator",
  "Role: midnight operator, signal medium, and logbook witness.",
  "Tune ghost traffic, triage strange messages, and survive the night shift.",
  {"Static Hour", "Numbers Rain", "Voice Beneath", "Sign-Off"},
  {"Tune the band until the station stops broadcasting your own breathing.", "Decode the number rain before it names someone still alive.", "Answer the voice beneath the carrier without inviting it into the room.", "Sign off cleanly and leave the next operator a survivable log."},
  {"The station sits where three county lines disagree and compasses get polite.", "Number rain is never random. It is only waiting for a scared listener.", "The voice beneath the carrier borrows words from lost emergency calls.", "A proper sign-off thanks the living first and the dead only if necessary."},
  {"Calm", "Focus", "Ground", "Reach", "Tape"},
  {"Clean Clip", "Code Wheel", "Salt Wire", "Sign-Off"},
  {{"Tune", "The static parts into a road name nobody has used since the flood.", -4, -5, -1, 5, 1, 18, 1}, {"Record", "You catch a clean clip before the tape grows warm in your hand.", -2, -3, -3, 7, 2, 17, 2}, {"Ground", "Copper, salt, and steady breathing pull the room back into shape.", 10, 8, -4, 1, -1, 12, 4}, {"Reply", "Something answers in Morse, using the desk lamp as punctuation.", -8, -6, -2, 10, 3, 22, 8}},
  0xC81F
};

void setup() {
  beginStoryGame(profile);
}

void loop() {
  loopStoryGame();
}
