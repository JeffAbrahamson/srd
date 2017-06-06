/*
  Copyright 2011  Jeff Abrahamson

  This file is part of srd.

  srd is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  srd is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with srd.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <map>
#include <string>
#include <vector>

#include "srd.h"
#include "test_text.h"

using namespace srd;
using namespace std;

vector_string srd::test_text() {
  vector_string text;
  text.push_back(""); // Yes, test the empty string
  text.push_back("This is the forest primeval.");
  text.push_back("This is the forest primeval."); // Test that a dup is ok
  text.push_back(
      "This is the forest primeval."
      "The murmuring pines and the hemlocks,\n"
      "Bearded with moss, and in garments green, indistinct in the twilight,\n"
      "Stand like Druids of eld, with voices sad and prophetic,\n"
      "Stand like harpers hoar, with beards that rest on their bosoms."
      "Loud from its rocky caverns, the deep-voiced neighboring ocean"
      "Speaks, and in accents disconsolate answers the wail of the forest.");
  // This next also tests a high-ascii character
  text.push_back(
      "This is the forest primeval; but where are the hearts that beneath it"
      "Leaped like the roe, when he hears in the woodland the voice of the "
      "huntsman?"
      "Where is the thatch-roofed village, the home of Acadian farmers,--"
      "Men whose lives glided on like rivers that water the woodlands,"
      "Darkened by shadows of earth, but reflecting an image of heaven?"
      "Waste are those pleasant farms, and the farmers forever departed!"
      "Scattered like dust and leaves, when the mighty blasts of October"
      "Seize them, and whirl them aloft, and sprinkle them far o'er the ocean"
      "Naught but tradition remains of the beautiful village of Grand-Pré.");
  // Just finish the stanza
  text.push_back(
      "Ye who believe in affection that hopes, and endures, and is patient,"
      "Ye who believe in the beauty and strength of woman's devotion,"
      "List to the mournful tradition, still sung by the pines of the forest;"
      "List to a Tale of Love in Acadie, home of the happy.");
  text.push_back(""); // The empty string is still ok
  text.push_back("This is the forest primeval."); // Test that a dup later is ok
  text.push_back(
      "François I était roi de la France."
      "Une île, un mèl, eine übersetzung."); // Test some more accent characters
  return text;
}

map<string, string> srd::orderly_text() {
  map<string, string> text;
  text["A"] = "This is the forest primeval.";
  text["B"] = "The murmuring pines and the hemlocks,";
  text["C"] =
      "Bearded with moss, and in garments green, indistinct in the twilight,";
  text["D"] = "Stand like Druids of eld, with voices sad and prophetic,";
  text["E"] = "Stand like harpers hoar, with beards that rest on their bosoms.";
  text["F"] = "Loud from its rocky caverns, the deep-voiced neighboring ocean";
  text["G"] =
      "Speaks, and in accents disconsolate answers the wail of the forest.";
  text["H"] =
      "This is the forest primeval; but where are the hearts that beneath it";
  text["I"] = "Leaped like the roe, when he hears in the woodland the voice of "
              "the huntsman?";
  text["J"] =
      "Where is the thatch-roofed village, the home of Acadian farmers,--";
  text["K"] = "Men whose lives glided on like rivers that water the woodlands,";
  text["L"] =
      "Darkened by shadows of earth, but reflecting an image of heaven?";
  text["M"] =
      "Waste are those pleasant farms, and the farmers forever departed!";
  text["N"] =
      "Scattered like dust and leaves, when the mighty blasts of October";
  text["O"] =
      "Seize them, and whirl them aloft, and sprinkle them far o'er the ocean";
  text["P"] =
      "Naught but tradition remains of the beautiful village of Grand-Pré.";
  text["Q"] =
      "Ye who believe in affection that hopes, and endures, and is patient,";
  text["R"] = "Ye who believe in the beauty and strength of woman's devotion,";
  text["S"] =
      "List to the mournful tradition, still sung by the pines of the forest;";
  text["T"] = "List to a Tale of Love in Acadie, home of the happy.";

  return text;
}

/*
  Test corpus for case-insensitive search.
  Case sensitive, unique keys and values.
  Case insensitive, each key is repeated twice, as is each value.
*/
map<string, string> srd::case_text() {
  map<string, string> text;
  text["A"] = "The sun was shining on the sea";
  text["a"] = "the sun was shining on the sea";
  text["B"] = "Shining with all his might";
  text["b"] = "shining with all his might";
  text["C"] = "He did his very best to make the billows smooth and bright";
  text["c"] = "he DID his VERY best TO make THE billows SMOOTH and BRIGHT";
  text["D"] = "And this was odd because it was the middle of the night.";
  text["d"] = "the moon was shining sulkily";
  text["E"] = "The moon was shining sulkily";
  text["e"] = "And this was ODD because it was the middle of the night.";

  return text;
}
