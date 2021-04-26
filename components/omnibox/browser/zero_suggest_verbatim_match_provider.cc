// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/zero_suggest_verbatim_match_provider.h"

#include "components/omnibox/browser/autocomplete_provider_client.h"
#include "components/omnibox/browser/autocomplete_provider_listener.h"
#include "components/omnibox/browser/verbatim_match.h"

namespace {
// The relevance score for verbatim match.
// Must outrank the QueryTiles relevance score.
const int kVerbatimMatchRelevanceScore = 1600;
}  // namespace

ZeroSuggestVerbatimMatchProvider::ZeroSuggestVerbatimMatchProvider(
    AutocompleteProviderClient* client)
    : AutocompleteProvider(TYPE_ZERO_SUGGEST), client_(client) {}

ZeroSuggestVerbatimMatchProvider::~ZeroSuggestVerbatimMatchProvider() = default;

void ZeroSuggestVerbatimMatchProvider::Start(const AutocompleteInput& input,
                                             bool minimal_changes) {
  Stop(true, false);

  // Only offer verbatim match on a site visit (non-SRP, non-NTP).
  if (input.current_page_classification() != metrics::OmniboxEventProto::OTHER)
    return;

  // Only offer verbatim match after the user just focused the Omnibox,
  // or if the input field is empty.
  if (!((input.from_omnibox_focus()) ||
        (input.type() == metrics::OmniboxInputType::EMPTY))) {
    return;
  }
  // Do not offer verbatim match, if the Omnibox does not contain a valid URL.
  if (!input.current_url().is_valid())
    return;

  const auto& current_query = input.current_url().spec();
  const auto& current_title = input.current_title();

  AutocompleteInput verbatim_input = input;
  verbatim_input.set_prevent_inline_autocomplete(true);
  verbatim_input.set_allow_exact_keyword_match(false);

  AutocompleteMatch match =
      VerbatimMatchForURL(client_, verbatim_input, GURL(current_query),
                          current_title, nullptr, kVerbatimMatchRelevanceScore);
  match.provider = this;
  matches_.push_back(match);
}

void ZeroSuggestVerbatimMatchProvider::Stop(bool clear_cached_results,
                                            bool due_to_user_inactivity) {
  matches_.clear();
}
