// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/lookalikes/core/lookalike_url_util.h"

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/feature_list.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/singleton.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool.h"
#include "base/time/default_clock.h"
#include "components/lookalikes/core/features.h"
#include "components/security_state/core/features.h"
#include "components/url_formatter/spoof_checks/top_domains/top500_domains.h"
#include "components/url_formatter/spoof_checks/top_domains/top_domain_util.h"
#include "components/url_formatter/url_formatter.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/base/url_util.h"

namespace lookalikes {

const char kHistogramName[] = "NavigationSuggestion.Event";

}  // namespace lookalikes

namespace {

// Digits. Used for trimming domains in Edit Distance heuristic matches. Domains
// that only differ by trailing digits (e.g. a1.tld and a2.tld) are ignored.
const char kDigitChars[] = "0123456789";

// Minimum length of e2LD protected against target embedding. For example,
// foo.bar.baz.com-evil.com embeds foo.bar.baz.com, but we don't flag it since
// "baz" is shorter than kMinTargetE2LDLength.
const size_t kMinE2LDLengthForTargetEmbedding = 4;

// This list will be added to the static list of common words so common words
// could be added to the list using a flag if needed.
const base::FeatureParam<std::string> kAdditionalCommonWords{
    &lookalikes::features::kDetectTargetEmbeddingLookalikes,
    "additional_common_words", ""};

// We might not protect a domain whose e2LD is a common word in target embedding
// based on the TLD that is paired with it.
const char* kCommonWords[] = {"shop",  "jobs",     "live",   "info",  "study",
                              "asahi", "weather",  "health", "forum", "radio",
                              "ideal", "research", "france", "free",  "mobile",
                              "sky",   "ask"};

bool SkeletonsMatch(const url_formatter::Skeletons& skeletons1,
                    const url_formatter::Skeletons& skeletons2) {
  DCHECK(!skeletons1.empty());
  DCHECK(!skeletons2.empty());
  for (const std::string& skeleton1 : skeletons1) {
    if (base::Contains(skeletons2, skeleton1)) {
      return true;
    }
  }
  return false;
}

// Returns a site that the user has used before that the eTLD+1 in
// |domain_and_registry| may be attempting to spoof, based on skeleton
// comparison.
std::string GetMatchingSiteEngagementDomain(
    const std::vector<DomainInfo>& engaged_sites,
    const DomainInfo& navigated_domain) {
  DCHECK(!navigated_domain.domain_and_registry.empty());
  for (const DomainInfo& engaged_site : engaged_sites) {
    DCHECK(!engaged_site.domain_and_registry.empty());
    if (SkeletonsMatch(navigated_domain.skeletons, engaged_site.skeletons)) {
      return engaged_site.domain_and_registry;
    }
  }
  return std::string();
}

// Returns the first matching top domain with an edit distance of at most one
// to |domain_and_registry|. This search is done in lexicographic order on the
// top 500 suitable domains, instead of in order by popularity. This means that
// the resulting "similar" domain may not be the most popular domain that
// matches.
std::string GetSimilarDomainFromTop500(
    const DomainInfo& navigated_domain,
    const LookalikeTargetAllowlistChecker& target_allowlisted) {
  for (const std::string& navigated_skeleton : navigated_domain.skeletons) {
    for (const char* const top_domain_skeleton :
         top500_domains::kTop500EditDistanceSkeletons) {
      // kTop500EditDistanceSkeletons may include blank entries.
      if (strlen(top_domain_skeleton) == 0) {
        continue;
      }

      if (!IsEditDistanceAtMostOne(base::UTF8ToUTF16(navigated_skeleton),
                                   base::UTF8ToUTF16(top_domain_skeleton))) {
        continue;
      }

      const std::string top_domain =
          url_formatter::LookupSkeletonInTopDomains(
              top_domain_skeleton, url_formatter::SkeletonType::kFull)
              .domain;
      DCHECK(!top_domain.empty());

      if (IsLikelyEditDistanceFalsePositive(navigated_domain,
                                            GetDomainInfo(top_domain))) {
        continue;
      }

      // Skip past domains that are allowed to be spoofed.
      if (target_allowlisted.Run(top_domain)) {
        continue;
      }

      return top_domain;
    }
  }
  return std::string();
}

// Returns the first matching engaged domain with an edit distance of at most
// one to |domain_and_registry|.
std::string GetSimilarDomainFromEngagedSites(
    const DomainInfo& navigated_domain,
    const std::vector<DomainInfo>& engaged_sites,
    const LookalikeTargetAllowlistChecker& target_allowlisted) {
  for (const std::string& navigated_skeleton : navigated_domain.skeletons) {
    for (const DomainInfo& engaged_site : engaged_sites) {
      if (!url_formatter::top_domains::IsEditDistanceCandidate(
              engaged_site.domain_and_registry)) {
        continue;
      }
      for (const std::string& engaged_skeleton : engaged_site.skeletons) {
        if (!IsEditDistanceAtMostOne(base::UTF8ToUTF16(navigated_skeleton),
                                     base::UTF8ToUTF16(engaged_skeleton))) {
          continue;
        }

        if (IsLikelyEditDistanceFalsePositive(navigated_domain, engaged_site)) {
          continue;
        }

        // Skip past domains that are allowed to be spoofed.
        if (target_allowlisted.Run(engaged_site.domain_and_registry)) {
          continue;
        }

        return engaged_site.domain_and_registry;
      }
    }
  }
  return std::string();
}

void RecordEvent(NavigationSuggestionEvent event) {
  UMA_HISTOGRAM_ENUMERATION(lookalikes::kHistogramName, event);
}

// Returns the parts of the domain that are separated by "." or "-", not
// including the eTLD.
//
// |host_without_etld| must outlive the return value since the vector contains
// StringPieces.
std::vector<base::StringPiece> SplitDomainWithouteTLDIntoTokens(
    const std::string& host_without_etld) {
  return base::SplitStringPiece(host_without_etld, "-.", base::TRIM_WHITESPACE,
                                base::SPLIT_WANT_NONEMPTY);
}

// Returns whether any subdomain ending in the last entry of |domain_labels| is
// allowlisted. e.g. if domain_labels = {foo,scholar,google,com}, checks the
// allowlist for google.com, scholar.google.com, and foo.scholar.google.com.
bool ASubdomainIsAllowlisted(
    const base::span<const base::StringPiece>& domain_labels,
    const LookalikeTargetAllowlistChecker& in_target_allowlist) {
  DCHECK(domain_labels.size() >= 2);
  std::string potential_hostname =
      domain_labels[domain_labels.size() - 1].as_string();
  // Attach each token from the end to the embedded target to check if that
  // subdomain has been allowlisted.
  for (int i = domain_labels.size() - 2; i >= 0; i--) {
    potential_hostname =
        domain_labels[i].as_string() + "." + potential_hostname;
    if (in_target_allowlist.Run(potential_hostname)) {
      return true;
    }
  }
  return false;
}

// Returns the top domain if the top domain without its separators matches the
// |potential_target| (e.g. googlecom). The matching is a skeleton matching.
std::string GetMatchingTopDomainWithoutSeparators(
    const base::StringPiece& potential_target) {
  const url_formatter::Skeletons skeletons =
      url_formatter::GetSkeletons(base::UTF8ToUTF16(potential_target));

  for (const auto& skeleton : skeletons) {
    url_formatter::TopDomainEntry matched_domain =
        url_formatter::LookupSkeletonInTopDomains(
            skeleton, url_formatter::SkeletonType::kSeparatorsRemoved);
    if (!matched_domain.domain.empty() &&
        matched_domain.skeleton_type ==
            url_formatter::SkeletonType::kSeparatorsRemoved) {
      return matched_domain.domain;
    }
  }
  return std::string();
}

// Returns if |etld_plus_one| shares the skeleton of an eTLD+1 with an engaged
// site or a top 500 domain. |embedded_target| is set to matching eTLD+1.
bool DoesETLDPlus1MatchTopDomainOrEngagedSite(
    const DomainInfo& domain,
    const std::vector<DomainInfo>& engaged_sites,
    std::string* embedded_target) {
  for (const auto& skeleton : domain.skeletons) {
    for (const auto& engaged_site : engaged_sites) {
      if (base::Contains(engaged_site.skeletons, skeleton)) {
        *embedded_target = engaged_site.domain_and_registry;
        return true;
      }
    }
  }
  for (const auto& skeleton : domain.skeletons) {
    const url_formatter::TopDomainEntry top_domain =
        url_formatter::LookupSkeletonInTopDomains(
            skeleton, url_formatter::SkeletonType::kFull);
    if (!top_domain.domain.empty() && top_domain.is_top_500) {
      *embedded_target = top_domain.domain;
      return true;
    }
  }
  return false;
}

// Returns whether the provided token includes a common word, which is a common
// indication of a likely false positive.
bool UsesCommonWord(const DomainInfo& domain) {
  std::vector<std::string> additional_common_words =
      base::SplitString(kAdditionalCommonWords.Get(), ",",
                        base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  if (base::Contains(additional_common_words, domain.domain_without_registry)) {
    return true;
  }
  for (auto* common_word : kCommonWords) {
    if (domain.domain_without_registry == common_word) {
      return true;
    }
  }
  return false;
}

// A domain is allowed to be embedded if its e2LD is a common word or any
// valid partial subdomain is allowlisted.
bool IsAllowedToBeEmbedded(
    const DomainInfo& embedded_target,
    const base::span<const base::StringPiece>& subdomain_span,
    const LookalikeTargetAllowlistChecker& in_target_allowlist) {
  return UsesCommonWord(embedded_target) ||
         ASubdomainIsAllowlisted(subdomain_span, in_target_allowlist);
}

}  // namespace

DomainInfo::DomainInfo(const std::string& arg_hostname,
                       const std::string& arg_domain_and_registry,
                       const std::string& arg_domain_without_registry,
                       const url_formatter::IDNConversionResult& arg_idn_result,
                       const url_formatter::Skeletons& arg_skeletons)
    : hostname(arg_hostname),
      domain_and_registry(arg_domain_and_registry),
      domain_without_registry(arg_domain_without_registry),
      idn_result(arg_idn_result),
      skeletons(arg_skeletons) {}

DomainInfo::~DomainInfo() = default;

DomainInfo::DomainInfo(const DomainInfo&) = default;

DomainInfo GetDomainInfo(const std::string& hostname) {
  if (net::HostStringIsLocalhost(hostname) ||
      net::IsHostnameNonUnique(hostname)) {
    return DomainInfo(std::string(), std::string(), std::string(),
                      url_formatter::IDNConversionResult(),
                      url_formatter::Skeletons());
  }
  const std::string domain_and_registry = GetETLDPlusOne(hostname);
  const std::string domain_without_registry =
      domain_and_registry.empty()
          ? std::string()
          : url_formatter::top_domains::HostnameWithoutRegistry(
                domain_and_registry);

  // eTLD+1 can be empty for private domains.
  if (domain_and_registry.empty()) {
    return DomainInfo(hostname, domain_and_registry, domain_without_registry,
                      url_formatter::IDNConversionResult(),
                      url_formatter::Skeletons());
  }
  // Compute skeletons using eTLD+1, skipping all spoofing checks. Spoofing
  // checks in url_formatter can cause the converted result to be punycode.
  // We want to avoid this in order to get an accurate skeleton for the unicode
  // version of the domain.
  const url_formatter::IDNConversionResult idn_result =
      url_formatter::UnsafeIDNToUnicodeWithDetails(domain_and_registry);
  const url_formatter::Skeletons skeletons =
      url_formatter::GetSkeletons(idn_result.result);
  return DomainInfo(hostname, domain_and_registry, domain_without_registry,
                    idn_result, skeletons);
}

DomainInfo GetDomainInfo(const GURL& url) {
  return GetDomainInfo(url.host());
}

std::string GetETLDPlusOne(const std::string& hostname) {
  return net::registry_controlled_domains::GetDomainAndRegistry(
      hostname, net::registry_controlled_domains::EXCLUDE_PRIVATE_REGISTRIES);
}

bool IsEditDistanceAtMostOne(const base::string16& str1,
                             const base::string16& str2) {
  if (str1.size() > str2.size() + 1 || str2.size() > str1.size() + 1) {
    return false;
  }
  base::string16::const_iterator i = str1.begin();
  base::string16::const_iterator j = str2.begin();
  size_t edit_count = 0;
  while (i != str1.end() && j != str2.end()) {
    if (*i == *j) {
      i++;
      j++;
    } else {
      edit_count++;
      if (edit_count > 1) {
        return false;
      }

      if (str1.size() > str2.size()) {
        // First string is longer than the second. This can only happen if the
        // first string has an extra character.
        i++;
      } else if (str2.size() > str1.size()) {
        // Second string is longer than the first. This can only happen if the
        // second string has an extra character.
        j++;
      } else {
        // Both strings are the same length. This can only happen if the two
        // strings differ by a single character.
        i++;
        j++;
      }
    }
  }
  if (i != str1.end() || j != str2.end()) {
    // A character at the end did not match.
    edit_count++;
  }
  return edit_count <= 1;
}

bool IsLikelyEditDistanceFalsePositive(const DomainInfo& navigated_domain,
                                       const DomainInfo& matched_domain) {
  DCHECK(url_formatter::top_domains::IsEditDistanceCandidate(
      matched_domain.domain_and_registry));
  DCHECK(url_formatter::top_domains::IsEditDistanceCandidate(
      navigated_domain.domain_and_registry));
  // If the only difference between the domains is the registry part, this is
  // unlikely to be a spoofing attempt and we should ignore this match.  E.g.
  // exclude matches like google.com.tw and google.com.tr.
  if (navigated_domain.domain_without_registry ==
      matched_domain.domain_without_registry) {
    return true;
  }

  // If the domains only differ by a numeric suffix on their e2LD (e.g.
  // site45.tld and site35.tld), then ignore the match.
  auto nav_trimmed = base::TrimString(navigated_domain.domain_without_registry,
                                      kDigitChars, base::TRIM_TRAILING);
  auto matched_trimmed = base::TrimString(
      matched_domain.domain_without_registry, kDigitChars, base::TRIM_TRAILING);
  DCHECK_NE(navigated_domain.domain_without_registry,
            matched_domain.domain_without_registry);
  // We previously verified that the domains without registries weren't equal,
  // so if they're equal now, the match must have come from numeric suffixes.
  if (nav_trimmed == matched_trimmed) {
    return true;
  }

  // Ignore domains that only differ by an insertion/substitution at the
  // start, as these are usually different words, not lookalikes.
  const auto nav_dom_len = navigated_domain.domain_and_registry.length();
  const auto matched_dom_len = matched_domain.domain_and_registry.length();
  const auto& nav_dom = navigated_domain.domain_and_registry;
  const auto& matched_dom = matched_domain.domain_and_registry;
  if (nav_dom_len == matched_dom_len) {
    // e.g. hank vs tank
    if (nav_dom.substr(1) == matched_dom.substr(1)) {
      return true;
    }
  } else if (nav_dom_len < matched_dom_len) {
    // e.g. oodle vs poodle
    if (nav_dom == matched_dom.substr(1)) {
      return true;
    }
  } else {  // navigated_dom_len > matched_dom_len
    // e.g. poodle vs oodle
    if (nav_dom.substr(1) == matched_dom) {
      return true;
    }
  }

  return false;
}

bool IsTopDomain(const DomainInfo& domain_info) {
  // Top domains are only accessible through their skeletons, so query the top
  // domains trie for each skeleton of this domain.
  for (const std::string& skeleton : domain_info.skeletons) {
    const url_formatter::TopDomainEntry top_domain =
        url_formatter::LookupSkeletonInTopDomains(
            skeleton, url_formatter::SkeletonType::kFull);
    if (domain_info.domain_and_registry == top_domain.domain) {
      return true;
    }
  }
  return false;
}

bool ShouldBlockLookalikeUrlNavigation(LookalikeUrlMatchType match_type,
                                       const DomainInfo& navigated_domain) {
  if (match_type == LookalikeUrlMatchType::kSiteEngagement) {
    return true;
  }
  if (match_type == LookalikeUrlMatchType::kTargetEmbedding &&
      base::FeatureList::IsEnabled(
          lookalikes::features::kDetectTargetEmbeddingLookalikes)) {
    return true;
  }
  return match_type == LookalikeUrlMatchType::kSkeletonMatchTop500;
}

bool GetMatchingDomain(
    const DomainInfo& navigated_domain,
    const std::vector<DomainInfo>& engaged_sites,
    const LookalikeTargetAllowlistChecker& in_target_allowlist,
    std::string* matched_domain,
    LookalikeUrlMatchType* match_type) {
  DCHECK(!navigated_domain.domain_and_registry.empty());
  DCHECK(matched_domain);
  DCHECK(match_type);

  if (navigated_domain.idn_result.has_idn_component) {
    // If the navigated domain is IDN, check its skeleton against engaged sites
    // and top domains.
    const std::string matched_engaged_domain =
        GetMatchingSiteEngagementDomain(engaged_sites, navigated_domain);
    DCHECK_NE(navigated_domain.domain_and_registry, matched_engaged_domain);
    if (!matched_engaged_domain.empty()) {
      *matched_domain = matched_engaged_domain;
      *match_type = LookalikeUrlMatchType::kSiteEngagement;
      return true;
    }

    if (!navigated_domain.idn_result.matching_top_domain.domain.empty()) {
      // In practice, this is not possible since the top domain list does not
      // contain IDNs, so domain_and_registry can't both have IDN and be a top
      // domain. Still, sanity check in case the top domain list changes in the
      // future.
      // At this point, navigated domain should not be a top domain.
      DCHECK_NE(navigated_domain.domain_and_registry,
                navigated_domain.idn_result.matching_top_domain.domain);
      *matched_domain = navigated_domain.idn_result.matching_top_domain.domain;
      *match_type = navigated_domain.idn_result.matching_top_domain.is_top_500
                        ? LookalikeUrlMatchType::kSkeletonMatchTop500
                        : LookalikeUrlMatchType::kSkeletonMatchTop5k;
      return true;
    }
  }

  if (url_formatter::top_domains::IsEditDistanceCandidate(
          navigated_domain.domain_and_registry)) {
    // If we can't find an exact top domain or an engaged site, try to find an
    // engaged domain within an edit distance of one.
    const std::string similar_engaged_domain = GetSimilarDomainFromEngagedSites(
        navigated_domain, engaged_sites, in_target_allowlist);
    if (!similar_engaged_domain.empty() &&
        navigated_domain.domain_and_registry != similar_engaged_domain) {
      *matched_domain = similar_engaged_domain;
      *match_type = LookalikeUrlMatchType::kEditDistanceSiteEngagement;
      return true;
    }

    // Finally, try to find a top domain within an edit distance of one.
    const std::string similar_top_domain =
        GetSimilarDomainFromTop500(navigated_domain, in_target_allowlist);
    if (!similar_top_domain.empty() &&
        navigated_domain.domain_and_registry != similar_top_domain) {
      *matched_domain = similar_top_domain;
      *match_type = LookalikeUrlMatchType::kEditDistance;
      return true;
    }
  }

  TargetEmbeddingType embedding_type =
      GetTargetEmbeddingType(navigated_domain.hostname, engaged_sites,
                             in_target_allowlist, matched_domain);
  if (embedding_type == TargetEmbeddingType::kSafetyTip) {
    *match_type = LookalikeUrlMatchType::kTargetEmbeddingForSafetyTips;
    return true;
  } else if (embedding_type == TargetEmbeddingType::kInterstitial) {
    *match_type = LookalikeUrlMatchType::kTargetEmbedding;
    return true;
  }

  DCHECK(embedding_type == TargetEmbeddingType::kNone);
  return false;
}

void RecordUMAFromMatchType(LookalikeUrlMatchType match_type) {
  switch (match_type) {
    case LookalikeUrlMatchType::kSiteEngagement:
      RecordEvent(NavigationSuggestionEvent::kMatchSiteEngagement);
      break;
    case LookalikeUrlMatchType::kEditDistance:
      RecordEvent(NavigationSuggestionEvent::kMatchEditDistance);
      break;
    case LookalikeUrlMatchType::kEditDistanceSiteEngagement:
      RecordEvent(NavigationSuggestionEvent::kMatchEditDistanceSiteEngagement);
      break;
    case LookalikeUrlMatchType::kTargetEmbedding:
      RecordEvent(NavigationSuggestionEvent::kMatchTargetEmbedding);
      break;
    case LookalikeUrlMatchType::kSkeletonMatchTop500:
      RecordEvent(NavigationSuggestionEvent::kMatchSkeletonTop500);
      break;
    case LookalikeUrlMatchType::kSkeletonMatchTop5k:
      RecordEvent(NavigationSuggestionEvent::kMatchSkeletonTop5k);
      break;
    case LookalikeUrlMatchType::kTargetEmbeddingForSafetyTips:
      RecordEvent(
          NavigationSuggestionEvent::kMatchTargetEmbeddingForSafetyTips);
      break;
    case LookalikeUrlMatchType::kFailedSpoofChecks:
      RecordEvent(NavigationSuggestionEvent::kFailedSpoofChecks);
      break;
    case LookalikeUrlMatchType::kNone:
      break;
  }
}

TargetEmbeddingType GetTargetEmbeddingType(
    const std::string& hostname,
    const std::vector<DomainInfo>& engaged_sites,
    const LookalikeTargetAllowlistChecker& in_target_allowlist,
    std::string* safe_hostname) {
  const std::string host_without_etld =
      url_formatter::top_domains::HostnameWithoutRegistry(hostname);
  const std::vector<base::StringPiece> hostname_tokens_without_etld =
      SplitDomainWithouteTLDIntoTokens(host_without_etld);

  // There are O(n^2) potential target embeddings in a domain name. We want to
  // be comprehensive, but optimize so that usually we needn't check all of
  // them. We do that by sweeping from the back of the embedding domain, towards
  // the front, checking for a valid eTLD. If we find one, then we consider the
  // possible embedded domains that end in that eTLD (i.e. all possible start
  // points from the beginning of the string onward).
  for (int end = hostname_tokens_without_etld.size(); end > 0; --end) {
    base::span<const base::StringPiece> etld_check_span(
        hostname_tokens_without_etld.data(), end);
    std::string etld_check_host = base::JoinString(etld_check_span, ".");
    auto etld_check_dominfo = GetDomainInfo(etld_check_host);

    // Check if the final token is a no-separator target (e.g. "googlecom").
    // This check happens first so that we can exclude invalid eTLD+1s next.
    std::string embedded_target = GetMatchingTopDomainWithoutSeparators(
        hostname_tokens_without_etld[end - 1]);
    if (!embedded_target.empty() &&
        !IsAllowedToBeEmbedded(etld_check_dominfo, etld_check_span,
                               in_target_allowlist)) {
      *safe_hostname = embedded_target;
      return TargetEmbeddingType::kInterstitial;
    }

    // Exclude otherwise-invalid eTLDs.
    if (etld_check_dominfo.domain_without_registry.empty()) {
      continue;
    }

    // Exclude e2LDs that are too short. <= because domain_without_registry has
    // a trailing ".".
    if (etld_check_dominfo.domain_without_registry.length() <=
        kMinE2LDLengthForTargetEmbedding) {
      continue;
    }

    // Check for exact matches against engaged sites, among all possible
    // subdomains ending at |end|.
    for (int start = 0; start < end - 1; ++start) {
      const base::span<const base::StringPiece> span(
          (hostname_tokens_without_etld.data() + start), end - start);
      auto embedded_hostname = base::JoinString(span, ".");
      auto embedded_dominfo = GetDomainInfo(embedded_hostname);

      for (auto& engaged_site : engaged_sites) {
        if (engaged_site.hostname == embedded_dominfo.hostname &&
            !IsAllowedToBeEmbedded(embedded_dominfo, span,
                                   in_target_allowlist)) {
          *safe_hostname = engaged_site.hostname;
          return TargetEmbeddingType::kInterstitial;
        }
      }
    }

    // There were no exact engaged site matches, but there may yet still be a
    // match against the eTLD+1 of an engaged or top site.
    if (DoesETLDPlus1MatchTopDomainOrEngagedSite(
            etld_check_dominfo, engaged_sites, safe_hostname) &&
        !IsAllowedToBeEmbedded(etld_check_dominfo, etld_check_span,
                               in_target_allowlist)) {
      return TargetEmbeddingType::kInterstitial;
    }
  }
  return TargetEmbeddingType::kNone;
}
