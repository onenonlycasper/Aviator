// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOCOMPLETE_AUTOCOMPLETE_MATCH_H_
#define CHROME_BROWSER_AUTOCOMPLETE_AUTOCOMPLETE_MATCH_H_

#include <map>
#include <string>
#include <vector>

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/search_engines/template_url.h"
#include "chrome/common/autocomplete_match_type.h"
#include "content/public/common/page_transition_types.h"
#include "url/gurl.h"

class AutocompleteProvider;
class Profile;
class TemplateURL;

namespace base {
class Time;
}  // namespace base

const char kACMatchPropertyInputText[] = "input text";
const char kACMatchPropertyContentsPrefix[] = "match contents prefix";
const char kACMatchPropertyContentsStartIndex[] = "match contents start index";

// AutocompleteMatch ----------------------------------------------------------

// A single result line with classified spans.  The autocomplete popup displays
// the 'contents' and the 'description' (the description is optional) in the
// autocomplete dropdown, and fills in 'fill_into_edit' into the textbox when
// that line is selected.  fill_into_edit may be the same as 'description' for
// things like URLs, but may be different for searches or other providers.  For
// example, a search result may say "Search for asdf" as the description, but
// "asdf" should appear in the box.
struct AutocompleteMatch {
  // Autocomplete matches contain strings that are classified according to a
  // separate vector of styles.  This vector associates flags with particular
  // string segments, and must be in sorted order.  All text must be associated
  // with some kind of classification.  Even if a match has no distinct
  // segments, its vector should contain an entry at offset 0 with no flags.
  //
  // Example: The user typed "goog"
  //   http://www.google.com/        Google
  //   ^          ^   ^              ^   ^
  //   0,         |   15,            |   4,
  //              11,match           0,match
  //
  // This structure holds the classification information for each span.
  struct ACMatchClassification {
    // The values in here are not mutually exclusive -- use them like a
    // bitfield.  This also means we use "int" instead of this enum type when
    // passing the values around, so the compiler doesn't complain.
    enum Style {
      NONE  = 0,
      URL   = 1 << 0,  // A URL
      MATCH = 1 << 1,  // A match for the user's search term
      DIM   = 1 << 2,  // "Helper text"
    };

    ACMatchClassification(size_t offset, int style)
        : offset(offset),
          style(style) {
    }

    // Offset within the string that this classification starts
    size_t offset;

    int style;
  };

  typedef std::vector<ACMatchClassification> ACMatchClassifications;

  // Type used by providers to attach additional, optional information to
  // an AutocompleteMatch.
  typedef std::map<std::string, std::string> AdditionalInfo;

  // The type of this match.
  typedef AutocompleteMatchType::Type Type;

  // Null-terminated array of characters that are not valid within |contents|
  // and |description| strings.
  static const base::char16 kInvalidChars[];

  AutocompleteMatch();
  AutocompleteMatch(AutocompleteProvider* provider,
                    int relevance,
                    bool deletable,
                    Type type);
  AutocompleteMatch(const AutocompleteMatch& match);
  ~AutocompleteMatch();

  // Converts |type| to a string representation.  Used in logging and debugging.
  AutocompleteMatch& operator=(const AutocompleteMatch& match);

  // Converts |type| to a resource identifier for the appropriate icon for this
  // type to show in the completion popup.
  static int TypeToIcon(Type type);

  // Converts |type| to a resource identifier for the appropriate icon for this
  // type to show in the location bar.
  static int TypeToLocationBarIcon(Type type);

  // Comparison function for determining when one match is better than another.
  static bool MoreRelevant(const AutocompleteMatch& elem1,
                           const AutocompleteMatch& elem2);

  // Comparison function for removing matches with duplicate destinations.
  // Destinations are compared using |stripped_destination_url|.  Pairs of
  // matches with empty destinations are treated as differing, since empty
  // destinations are expected for non-navigable matches.
  static bool DestinationsEqual(const AutocompleteMatch& elem1,
                                const AutocompleteMatch& elem2);

  // Helper functions for classes creating matches:
  // Fills in the classifications for |text|, using |style| as the base style
  // and marking the first instance of |find_text| as a match.  (This match
  // will also not be dimmed, if |style| has DIM set.)
  static void ClassifyMatchInString(const base::string16& find_text,
                                    const base::string16& text,
                                    int style,
                                    ACMatchClassifications* classifications);

  // Similar to ClassifyMatchInString(), but for cases where the range to mark
  // as matching is already known (avoids calling find()).  This can be helpful
  // when find() would be misleading (e.g. you want to mark the second match in
  // a string instead of the first).
  static void ClassifyLocationInString(size_t match_location,
                                       size_t match_length,
                                       size_t overall_length,
                                       int style,
                                       ACMatchClassifications* classifications);

  // Returns a new vector of classifications containing the merged contents of
  // |classifications1| and |classifications2|.
  static ACMatchClassifications MergeClassifications(
      const ACMatchClassifications& classifications1,
      const ACMatchClassifications& classifications2);

  // Converts classifications to and from a serialized string representation
  // (using comma-separated integers to sequentially list positions and styles).
  static std::string ClassificationsToString(
      const ACMatchClassifications& classifications);
  static ACMatchClassifications ClassificationsFromString(
      const std::string& serialized_classifications);

  // Adds a classification to the end of |classifications| iff its style is
  // different from the last existing classification.  |offset| must be larger
  // than the offset of the last classification in |classifications|.
  static void AddLastClassificationIfNecessary(
      ACMatchClassifications* classifications,
      size_t offset,
      int style);

  // Removes invalid characters from |text|. Should be called on strings coming
  // from external sources (such as extensions) before assigning to |contents|
  // or |description|.
  static base::string16 SanitizeString(const base::string16& text);

  // Convenience function to check if |type| is a search (as opposed to a URL or
  // an extension).
  static bool IsSearchType(Type type);

  // Convenience function to check if |type| is a special search suggest type -
  // like entity, personalized, profile or postfix.
  static bool IsSpecializedSearchType(Type type);

  // Copies the destination_url with "www." stripped off to
  // |stripped_destination_url| and also converts https protocol to
  // http.  These two conversions are merely to allow comparisons to
  // remove likely duplicates; these URLs are not used as actual
  // destination URLs.  This method is invoked internally by the
  // AutocompleteResult and does not normally need to be invoked.
  // If |profile| is not NULL, it is used to get a template URL corresponding
  // to this match.  The template is used to strip off query args other than
  // the search terms themselves that would otherwise prevent from proper
  // deduping.
  void ComputeStrippedDestinationURL(Profile* profile);

  // Gets data relevant to whether there should be any special keyword-related
  // UI shown for this match.  If this match represents a selected keyword, i.e.
  // the UI should be "in keyword mode", |keyword| will be set to the keyword
  // and |is_keyword_hint| will be set to false.  If this match has a non-NULL
  // |associated_keyword|, i.e. we should show a "Press [tab] to search ___"
  // hint and allow the user to toggle into keyword mode, |keyword| will be set
  // to the associated keyword and |is_keyword_hint| will be set to true.  Note
  // that only one of these states can be in effect at once.  In all other
  // cases, |keyword| will be cleared, even when our member variable |keyword|
  // is non-empty -- such as with non-substituting keywords or matches that
  // represent searches using the default search engine.  See also
  // GetSubstitutingExplicitlyInvokedKeyword().
  void GetKeywordUIState(Profile* profile,
                         base::string16* keyword,
                         bool* is_keyword_hint) const;

  // Returns |keyword|, but only if it represents a substituting keyword that
  // the user has explicitly invoked.  If for example this match represents a
  // search with the default search engine (and the user didn't explicitly
  // invoke its keyword), this returns the empty string.  The result is that
  // this function returns a non-empty string in the same cases as when the UI
  // should show up as being "in keyword mode".
  base::string16 GetSubstitutingExplicitlyInvokedKeyword(
      Profile* profile) const;

  // Returns the TemplateURL associated with this match.  This may be NULL if
  // the match has no keyword OR if the keyword no longer corresponds to a valid
  // TemplateURL.  See comments on |keyword| below.
  // If |allow_fallback_to_destination_host| is true and the keyword does
  // not map to a valid TemplateURL, we'll then check for a TemplateURL that
  // corresponds to the destination_url's hostname.
  TemplateURL* GetTemplateURL(Profile* profile,
                              bool allow_fallback_to_destination_host) const;

  // Adds optional information to the |additional_info| dictionary.
  void RecordAdditionalInfo(const std::string& property,
                            const std::string& value);
  void RecordAdditionalInfo(const std::string& property, int value);
  void RecordAdditionalInfo(const std::string& property,
                            const base::Time& value);

  // Returns the value recorded for |property| in the |additional_info|
  // dictionary.  Returns the empty string if no such value exists.
  std::string GetAdditionalInfo(const std::string& property) const;

  // Returns whether this match is a "verbatim" match: a URL navigation directly
  // to the user's input, a search for the user's input with the default search
  // engine, or a "keyword mode" search for the query portion of the user's
  // input.  Note that rare or unusual types that could be considered verbatim,
  // such as keyword engine matches or extension-provided matches, aren't
  // detected by this IsVerbatimType, as the user will not be able to infer
  // what will happen when he or she presses enter in those cases if the match
  // is not shown.
  bool IsVerbatimType() const;

  // Returns whether this match or any duplicate of this match can be deleted.
  // This is used to decide whether we should call DeleteMatch().
  bool SupportsDeletion() const;

  // The provider of this match, used to remember which provider the user had
  // selected when the input changes. This may be NULL, in which case there is
  // no provider (or memory of the user's selection).
  AutocompleteProvider* provider;

  // The relevance of this match. See table in autocomplete.h for scores
  // returned by various providers. This is used to rank matches among all
  // responding providers, so different providers must be carefully tuned to
  // supply matches with appropriate relevance.
  //
  // TODO(pkasting): http://b/1111299 This should be calculated algorithmically,
  // rather than being a fairly fixed value defined by the table above.
  int relevance;

  // How many times this result was typed in / selected from the omnibox.
  // Only set for some providers and result_types.  If it is not set,
  // its value is -1.  At the time of writing this comment, it is only
  // set for matches from HistoryURL and HistoryQuickProvider.
  int typed_count;

  // True if the user should be able to delete this match.
  bool deletable;

  // This string is loaded into the location bar when the item is selected
  // by pressing the arrow keys. This may be different than a URL, for example,
  // for search suggestions, this would just be the search terms.
  base::string16 fill_into_edit;

  // The inline autocompletion to display after the user's typing in the
  // omnibox, if this match becomes the default match.  It may be empty.
  base::string16 inline_autocompletion;

  // If false, the omnibox should prevent this match from being the
  // default match.  Providers should set this to true only if the
  // user's input, plus any inline autocompletion on this match, would
  // lead the user to expect a navigation to this match's destination.
  // For example, with input "foo", a search for "bar" or navigation
  // to "bar.com" should not set this flag; a navigation to "foo.com"
  // should only set this flag if ".com" will be inline autocompleted;
  // and a navigation to "foo/" (an intranet host) or search for "foo"
  // should set this flag.
  bool allowed_to_be_default_match;

  // The URL to actually load when the autocomplete item is selected. This URL
  // should be canonical so we can compare URLs with strcmp to avoid dupes.
  // It may be empty if there is no possible navigation.
  GURL destination_url;

  // The destination URL with "www." stripped off for better dupe finding.
  GURL stripped_destination_url;

  // The main text displayed in the address bar dropdown.
  base::string16 contents;
  ACMatchClassifications contents_class;

  // Additional helper text for each entry, such as a title or description.
  base::string16 description;
  ACMatchClassifications description_class;

  // A rich-format version of the display for the dropdown.
  base::string16 answer_contents;
  base::string16 answer_type;

  // The transition type to use when the user opens this match.  By default
  // this is TYPED.  Providers whose matches do not look like URLs should set
  // it to GENERATED.
  content::PageTransition transition;

  // True when this match is the "what you typed" match from the history
  // system.
  bool is_history_what_you_typed_match;

  // Type of this match.
  Type type;

  // Set with a keyword provider match if this match can show a keyword hint.
  // For example, if this is a SearchProvider match for "www.amazon.com",
  // |associated_keyword| could be a KeywordProvider match for "amazon.com".
  scoped_ptr<AutocompleteMatch> associated_keyword;

  // The keyword of the TemplateURL the match originated from.  This is nonempty
  // for both explicit "keyword mode" matches as well as matches for the default
  // search provider (so, any match for which we're doing substitution); it
  // doesn't imply (alone) that the UI is going to show a keyword hint or
  // keyword mode.  For that, see GetKeywordUIState() or
  // GetSubstitutingExplicitlyInvokedKeyword().
  //
  // CAUTION: The TemplateURL associated with this keyword may be deleted or
  // modified while the AutocompleteMatch is alive.  This means anyone who
  // accesses it must perform any necessary sanity checks before blindly using
  // it!
  base::string16 keyword;

  // True if the user has starred the destination URL.
  bool starred;

  // True if this match is from a previous result.
  bool from_previous;

  // Optional search terms args.  If present,
  // AutocompleteController::UpdateAssistedQueryStats() will incorporate this
  // data with additional data it calculates and pass the completed struct to
  // TemplateURLRef::ReplaceSearchTerms() to reset the match's |destination_url|
  // after the complete set of matches in the AutocompleteResult has been chosen
  // and sorted.  Most providers will leave this as NULL, which will cause the
  // AutocompleteController to do no additional transformations.
  scoped_ptr<TemplateURLRef::SearchTermsArgs> search_terms_args;

  // Information dictionary into which each provider can optionally record a
  // property and associated value and which is presented in aviator://omnibox.
  AdditionalInfo additional_info;

  // A list of matches culled during de-duplication process, retained to
  // ensure if a match is deleted, the duplicates are deleted as well.
  std::vector<AutocompleteMatch> duplicate_matches;

#ifndef NDEBUG
  // Does a data integrity check on this match.
  void Validate() const;

  // Checks one text/classifications pair for valid values.
  void ValidateClassifications(
      const base::string16& text,
      const ACMatchClassifications& classifications) const;
#endif
};

typedef AutocompleteMatch::ACMatchClassification ACMatchClassification;
typedef std::vector<ACMatchClassification> ACMatchClassifications;
typedef std::vector<AutocompleteMatch> ACMatches;

#endif  // CHROME_BROWSER_AUTOCOMPLETE_AUTOCOMPLETE_MATCH_H_
