// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/favicon/favicon_url_parser.h"

#include "base/strings/string_number_conversions.h"
#include "components/favicon_base/favicon_types.h"
#include "net/url_request/url_request.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/gfx/favicon_size.h"

namespace {

// Parameters which can be used in aviator://favicon path. See file
// "chrome/browser/ui/webui/favicon_source.h" for a description of
// what each does.
const char kIconURLParameter[] = "iconurl/";
const char kLargestParameter[] = "largest/";
const char kOriginParameter[] = "origin/";
const char kSizeParameter[] = "size/";

// Returns true if |search| is a substring of |path| which starts at
// |start_index|.
bool HasSubstringAt(const std::string& path,
                    size_t start_index,
                    const std::string& search) {
  return path.compare(start_index, search.length(), search) == 0;
}

}  // namespace

namespace chrome {

bool ParseFaviconPath(const std::string& path,
                      int icon_types,
                      ParsedFaviconPath* parsed) {
  parsed->is_icon_url = false;
  parsed->url = "";
  parsed->size_in_dip = gfx::kFaviconSize;
  parsed->device_scale_factor = 1.0f;
  parsed->path_index = -1;

  if (path.empty())
    return false;

  size_t parsed_index = 0;
  if (HasSubstringAt(path, parsed_index, kLargestParameter)) {
    parsed_index += strlen(kLargestParameter);
    parsed->size_in_dip = 0;
  } else if (HasSubstringAt(path, parsed_index, kSizeParameter)) {
    parsed_index += strlen(kSizeParameter);

    size_t slash = path.find("/", parsed_index);
    if (slash == std::string::npos)
      return false;

    size_t scale_delimiter = path.find("@", parsed_index);
    std::string size_str;
    std::string scale_str;
    if (scale_delimiter == std::string::npos) {
      // Support the legacy size format of 'size/aa/' where 'aa' is the desired
      // size in DIP for the sake of not regressing the extensions which use it.
      size_str = path.substr(parsed_index, slash - parsed_index);
    } else {
      size_str = path.substr(parsed_index, scale_delimiter - parsed_index);
      scale_str = path.substr(scale_delimiter + 1,
                              slash - scale_delimiter - 1);
    }

    if (!base::StringToInt(size_str, &parsed->size_in_dip))
      return false;

    if (parsed->size_in_dip != (gfx::kFaviconSize * 4) &&
        parsed->size_in_dip != (gfx::kFaviconSize * 2)) {
      // Only 64x64, 32x32 and 16x16 icons are supported.
      parsed->size_in_dip = gfx::kFaviconSize;
    }
    if (!scale_str.empty())
      webui::ParseScaleFactor(scale_str, &parsed->device_scale_factor);

    // Return the default favicon (as opposed to a resized favicon) for
    // favicon sizes which are not cached by the favicon service.
    // Currently the favicon service caches:
    // - favicons of sizes "gfx::kFaviconSize * scale factor" px of type FAVICON
    //   where scale factor is one of FaviconUtil::GetFaviconScales().
    // - the largest TOUCH_ICON / TOUCH_PRECOMPOSED_ICON
    if (parsed->size_in_dip != gfx::kFaviconSize &&
        icon_types == favicon_base::FAVICON)
      return false;

    parsed_index = slash + 1;
  }

  if (HasSubstringAt(path, parsed_index, kIconURLParameter)) {
    parsed_index += strlen(kIconURLParameter);
    parsed->is_icon_url = true;
    parsed->url = path.substr(parsed_index);
  } else {
    // URL requests prefixed with "origin/" are converted to a form with an
    // empty path and a valid scheme. (e.g., example.com -->
    // http://example.com/ or http://example.com/a --> http://example.com/)
    if (HasSubstringAt(path, parsed_index, kOriginParameter)) {
      parsed_index += strlen(kOriginParameter);
      std::string possibly_invalid_url = path.substr(parsed_index);

      // If the URL does not specify a scheme (e.g., example.com instead of
      // http://example.com), add "http://" as a default.
      if (!GURL(possibly_invalid_url).has_scheme())
        possibly_invalid_url = "http://" + possibly_invalid_url;

      // Strip the path beyond the top-level domain.
      parsed->url = GURL(possibly_invalid_url).GetOrigin().spec();
    } else {
      parsed->url = path.substr(parsed_index);
    }
  }

  // The parsed index needs to be returned in order to allow Instant Extended
  // to translate favicon URLs using advanced parameters.
  // Example:
  //   "chrome-search://favicon/size/16@2x/<renderer-id>/<most-visited-id>"
  // would be translated to:
  //   "chrome-search://favicon/size/16@2x/<most-visited-item-with-given-id>".
  parsed->path_index = parsed_index;
  return true;
}

}  // namespace chrome
