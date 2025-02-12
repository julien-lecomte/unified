/// @addtogroup spellchecker SpellChecker
/// @brief Functions related to spellchecking
/// @{
/// @file nwnx_spellcheck.nss

const string NWNX_SpellChecker = "NWNX_SpellChecker"; ///< @private

/// @brief Finds misspells in a string.
/// @param sentence The sentence to check.
/// @return The spelling mistakes in the sentence, comma delimited. Returns blank if no errors or if .so file
/// is improperly installed.
/// @note If it returns an error in every word, even when spelled correctly, the dictionary is not set up correctly.
/// @warning These functions can be performance heavy, do limit how many calls and/or how long of a sentence is passed.
/// Make use of **DelayCommands** and **AssignCommands**
string NWNX_SpellChecker_FindMisspell(string sentence);

/// @brief Get suggestions on a single word, comma delimited.
/// @param word The string to check for suggestions.
/// @return A comma delimited lists of suggestions for a word. Returns blank if no errors or if .so file is improperly
/// installed.
/// @warning These functions can be performance heavy, do limit how many calls and/or how long of a sentence is passed.
/// Make use of **DelayCommands** and **AssignCommands**
string NWNX_SpellChecker_GetSuggestSpell(string word);

/// @}

string NWNX_SpellChecker_FindMisspell(string sentence)
{
    NWNXPushString(sentence);
    NWNXCall(NWNX_SpellChecker, "FindMisspell");
    return NWNXPopString();
}

string NWNX_SpellChecker_GetSuggestSpell(string word)
{
    NWNXPushString(word);
    NWNXCall(NWNX_SpellChecker, "GetSuggestSpell");
    return NWNXPopString();
}
