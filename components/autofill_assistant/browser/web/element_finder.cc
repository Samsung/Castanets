// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/web/element_finder.h"

#include "components/autofill_assistant/browser/devtools/devtools_client.h"
#include "components/autofill_assistant/browser/service.pb.h"
#include "components/autofill_assistant/browser/web/web_controller_util.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"

namespace autofill_assistant {

namespace {
// Javascript code to get document root element.
const char kGetDocumentElement[] =
    "(function() { return document.documentElement; }())";

const char kGetArrayElement[] = "function(index) { return this[index]; }";

bool ConvertPseudoType(const PseudoType pseudo_type,
                       dom::PseudoType* pseudo_type_output) {
  switch (pseudo_type) {
    case PseudoType::UNDEFINED:
      break;
    case PseudoType::FIRST_LINE:
      *pseudo_type_output = dom::PseudoType::FIRST_LINE;
      return true;
    case PseudoType::FIRST_LETTER:
      *pseudo_type_output = dom::PseudoType::FIRST_LETTER;
      return true;
    case PseudoType::BEFORE:
      *pseudo_type_output = dom::PseudoType::BEFORE;
      return true;
    case PseudoType::AFTER:
      *pseudo_type_output = dom::PseudoType::AFTER;
      return true;
    case PseudoType::BACKDROP:
      *pseudo_type_output = dom::PseudoType::BACKDROP;
      return true;
    case PseudoType::SELECTION:
      *pseudo_type_output = dom::PseudoType::SELECTION;
      return true;
    case PseudoType::FIRST_LINE_INHERITED:
      *pseudo_type_output = dom::PseudoType::FIRST_LINE_INHERITED;
      return true;
    case PseudoType::SCROLLBAR:
      *pseudo_type_output = dom::PseudoType::SCROLLBAR;
      return true;
    case PseudoType::SCROLLBAR_THUMB:
      *pseudo_type_output = dom::PseudoType::SCROLLBAR_THUMB;
      return true;
    case PseudoType::SCROLLBAR_BUTTON:
      *pseudo_type_output = dom::PseudoType::SCROLLBAR_BUTTON;
      return true;
    case PseudoType::SCROLLBAR_TRACK:
      *pseudo_type_output = dom::PseudoType::SCROLLBAR_TRACK;
      return true;
    case PseudoType::SCROLLBAR_TRACK_PIECE:
      *pseudo_type_output = dom::PseudoType::SCROLLBAR_TRACK_PIECE;
      return true;
    case PseudoType::SCROLLBAR_CORNER:
      *pseudo_type_output = dom::PseudoType::SCROLLBAR_CORNER;
      return true;
    case PseudoType::RESIZER:
      *pseudo_type_output = dom::PseudoType::RESIZER;
      return true;
    case PseudoType::INPUT_LIST_BUTTON:
      *pseudo_type_output = dom::PseudoType::INPUT_LIST_BUTTON;
      return true;
  }
  return false;
}
}  // namespace

ElementFinder::JsFilterBuilder::JsFilterBuilder() = default;
ElementFinder::JsFilterBuilder::~JsFilterBuilder() = default;

std::vector<std::unique_ptr<runtime::CallArgument>>
ElementFinder::JsFilterBuilder::BuildArgumentList() const {
  auto str_array_arg = std::make_unique<base::Value>(base::Value::Type::LIST);
  for (const std::string& str : arguments_) {
    str_array_arg->Append(str);
  }
  std::vector<std::unique_ptr<runtime::CallArgument>> arguments;
  arguments.emplace_back(runtime::CallArgument::Builder()
                             .SetValue(std::move(str_array_arg))
                             .Build());
  return arguments;
}

// clang-format off
std::string ElementFinder::JsFilterBuilder::BuildFunction() const {
  return base::StrCat({
    R"(
    function(args) {
      let elements = [this];
    )",
    base::JoinString(lines_, "\n"),
    R"(
      if (elements.length == 0) return null;
      if (elements.length == 1) { return elements[0] }
      return elements;
    })"
  });
}
// clang-format on

bool ElementFinder::JsFilterBuilder::AddFilter(
    const SelectorProto::Filter& filter) {
  switch (filter.filter_case()) {
    case SelectorProto::Filter::kCssSelector:
      // clang-format off
      AddLine({
        "elements = elements.flatMap((e) => Array.from(e.querySelectorAll(",
        AddArgument(filter.css_selector()),
        ")));"
      });

      // Elements are temporarily put into a set to get rid of duplicates, which
      // are likely when using inner text before CSS selector filters. We must
      // not return duplicates as they cause incorrect TOO_MANY_ELEMENTS errors.
      AddLine(R"(if (elements.length > 1) {
        elements = Array.from(new Set(elements));
      })");
      // clang-format on
      return true;

    case SelectorProto::Filter::kInnerText:
      AddRegexpFilter(filter.inner_text(), "innerText");
      return true;

    case SelectorProto::Filter::kValue:
      AddRegexpFilter(filter.value(), "value");
      return true;

    case SelectorProto::Filter::kBoundingBox:
      AddLine(
          "elements = elements.filter((e) => e.getClientRects().length > 0);");
      return true;

    case SelectorProto::Filter::kPseudoElementContent: {
      // When a content is set, window.getComputedStyle().content contains a
      // double-quoted string with the content, unquoted here by JSON.parse().
      std::string re_var =
          AddRegexpInstance(filter.pseudo_element_content().content());
      std::string pseudo_type =
          PseudoTypeName(filter.pseudo_element_content().pseudo_type());

      AddLine("elements = elements.filter((e) => {");
      AddLine({"  const s = window.getComputedStyle(e, '", pseudo_type, "');"});
      AddLine("  if (!s || !s.content || !s.content.startsWith('\"')) {");
      AddLine("    return false;");
      AddLine("  }");
      AddLine({"  return ", re_var, ".test(JSON.parse(s.content));"});
      AddLine("});");
      return true;
    }

    case SelectorProto::Filter::kLabelled:
      AddLine(R"(elements = elements.flatMap((e) => {
  if (e.tagName != 'LABEL') return [];
  let element = null;
  const id = e.getAttribute('for');
  if (id) {
    element = document.getElementById(id)
  }
  if (!element) {
    element = e.querySelector(
      'button,input,keygen,meter,output,progress,select,textarea');
  }
  if (element) return [element];
  return [];
});
)");
      // The selector above for the case where there's no "for" corresponds to
      // the list of labelable elements listed on "W3C's HTML5: Edition for Web
      // Authors":
      // https://www.w3.org/TR/2011/WD-html5-author-20110809/forms.html#category-label
      return true;

    case SelectorProto::Filter::kEnterFrame:
    case SelectorProto::Filter::kPseudoType:
    case SelectorProto::Filter::kPickOne:
    case SelectorProto::Filter::kClosest:
    case SelectorProto::Filter::FILTER_NOT_SET:
      return false;
  }
}

std::string ElementFinder::JsFilterBuilder::AddRegexpInstance(
    const SelectorProto::TextFilter& filter) {
  std::string re_flags = filter.case_sensitive() ? "" : "i";
  std::string re_var = DeclareVariable();
  AddLine({"const ", re_var, " = RegExp(", AddArgument(filter.re2()), ", '",
           re_flags, "');"});
  return re_var;
}

void ElementFinder::JsFilterBuilder::AddRegexpFilter(
    const SelectorProto::TextFilter& filter,
    const std::string& property) {
  std::string re_var = AddRegexpInstance(filter);
  AddLine({"elements = elements.filter((e) => ", re_var, ".test(e.", property,
           "));"});
}

std::string ElementFinder::JsFilterBuilder::DeclareVariable() {
  return base::StrCat({"v", base::NumberToString(variable_counter_++)});
}

std::string ElementFinder::JsFilterBuilder::AddArgument(
    const std::string& value) {
  int index = arguments_.size();
  arguments_.emplace_back(value);
  return base::StrCat({"args[", base::NumberToString(index), "]"});
}

ElementFinder::Result::Result() = default;

ElementFinder::Result::~Result() = default;

ElementFinder::Result::Result(const Result&) = default;

ElementFinder::ElementFinder(content::WebContents* web_contents,
                             DevtoolsClient* devtools_client,
                             const Selector& selector,
                             ResultType result_type)
    : web_contents_(web_contents),
      devtools_client_(devtools_client),
      selector_(selector),
      result_type_(result_type) {}

ElementFinder::~ElementFinder() = default;

void ElementFinder::Start(Callback callback) {
  StartInternal(std::move(callback), web_contents_->GetMainFrame(),
                /* frame_id= */ "", /* document_object_id= */ "");
}

void ElementFinder::StartInternal(Callback callback,
                                  content::RenderFrameHost* frame,
                                  const std::string& frame_id,
                                  const std::string& document_object_id) {
  callback_ = std::move(callback);

  if (selector_.empty()) {
    SendResult(ClientStatus(INVALID_SELECTOR));
    return;
  }

  current_frame_ = frame;
  current_frame_id_ = frame_id;
  current_frame_root_ = document_object_id;
  if (current_frame_root_.empty()) {
    GetDocumentElement();
  } else {
    current_matches_.emplace_back(current_frame_root_);
    ExecuteNextTask();
  }
}

void ElementFinder::SendResult(const ClientStatus& status) {
  if (!callback_)
    return;

  std::move(callback_).Run(status, std::make_unique<Result>());
}

void ElementFinder::SendSuccessResult(const std::string& object_id) {
  if (!callback_)
    return;

  // Fill in result and return
  std::unique_ptr<Result> result =
      std::make_unique<Result>(BuildResult(object_id));
  result->frame_stack = frame_stack_;
  std::move(callback_).Run(OkClientStatus(), std::move(result));
}

ElementFinder::Result ElementFinder::BuildResult(const std::string& object_id) {
  Result result;
  result.container_frame_host = current_frame_;
  result.object_id = object_id;
  result.node_frame_id = current_frame_id_;
  return result;
}

void ElementFinder::ExecuteNextTask() {
  const auto& filters = selector_.proto.filters();

  if (next_filter_index_ >= filters.size()) {
    std::string object_id;
    switch (result_type_) {
      case ResultType::kExactlyOneMatch:
        if (!ConsumeOneMatchOrFail(object_id)) {
          return;
        }
        break;

      case ResultType::kAnyMatch:
        if (!ConsumeAnyMatchOrFail(object_id)) {
          return;
        }
        break;

      case ResultType::kMatchArray:
        if (!ConsumeMatchArrayOrFail(object_id)) {
          return;
        }
        break;
    }
    SendSuccessResult(object_id);
    return;
  }

  const auto& filter = filters.Get(next_filter_index_);
  switch (filter.filter_case()) {
    case SelectorProto::Filter::kEnterFrame: {
      std::string object_id;
      if (!ConsumeOneMatchOrFail(object_id))
        return;

      // The above fails if there is more than one frame. To preserve
      // backward-compatibility with the previous, lax behavior, callers must
      // add pick_one before enter_frame. TODO(b/155264465): allow searching in
      // more than one frame.
      next_filter_index_++;
      EnterFrame(object_id);
      return;
    }

    case SelectorProto::Filter::kPseudoType: {
      std::vector<std::string> matches;
      if (!ConsumeAllMatchesOrFail(matches))
        return;

      next_filter_index_++;
      matching_pseudo_elements_ = true;
      ResolvePseudoElement(filter.pseudo_type(), matches);
      return;
    }

    case SelectorProto::Filter::kPickOne: {
      std::string object_id;
      if (!ConsumeAnyMatchOrFail(object_id))
        return;

      next_filter_index_++;
      current_matches_ = {object_id};
      ExecuteNextTask();
      return;
    }

    case SelectorProto::Filter::kCssSelector:
    case SelectorProto::Filter::kInnerText:
    case SelectorProto::Filter::kValue:
    case SelectorProto::Filter::kBoundingBox:
    case SelectorProto::Filter::kPseudoElementContent:
    case SelectorProto::Filter::kLabelled: {
      std::vector<std::string> matches;
      if (!ConsumeAllMatchesOrFail(matches))
        return;

      JsFilterBuilder js_filter;
      for (int i = next_filter_index_; i < filters.size(); i++) {
        if (!js_filter.AddFilter(filters.Get(i))) {
          break;
        }
        next_filter_index_++;
      }
      ApplyJsFilters(js_filter, matches);
      return;
    }

    case SelectorProto::Filter::kClosest: {
      std::string array_object_id;
      if (!ConsumeMatchArrayOrFail(array_object_id))
        return;

      ApplyProximityFilter(next_filter_index_++, array_object_id);
      return;
    }

    case SelectorProto::Filter::FILTER_NOT_SET:
      VLOG(1) << __func__ << " Unset or unknown filter in " << filter << " in "
              << selector_;
      SendResult(ClientStatus(INVALID_SELECTOR));
      return;
  }
}

bool ElementFinder::ConsumeOneMatchOrFail(std::string& object_id_out) {
  // This logic relies on JsFilterBuilder::BuildFunction guaranteeing that
  // arrays contain at least 2 elements to avoid having to fetch all matching
  // elements in the common case where we just want to know whether there is at
  // least one match.

  if (!current_match_arrays_.empty()) {
    VLOG(1) << __func__ << " Got " << current_match_arrays_.size()
            << " arrays of 2 or more matches for " << selector_
            << ", when only 1 match was expected.";
    SendResult(ClientStatus(TOO_MANY_ELEMENTS));
    return false;
  }
  if (current_matches_.size() > 1) {
    VLOG(1) << __func__ << " Got " << current_matches_.size() << " matches for "
            << selector_ << ", when only 1 was expected.";
    SendResult(ClientStatus(TOO_MANY_ELEMENTS));
    return false;
  }
  if (current_matches_.empty()) {
    SendResult(ClientStatus(ELEMENT_RESOLUTION_FAILED));
    return false;
  }

  object_id_out = current_matches_[0];
  current_matches_.clear();
  return true;
}

bool ElementFinder::ConsumeAnyMatchOrFail(std::string& object_id_out) {
  // This logic relies on ApplyJsFilters guaranteeing that arrays contain at
  // least 2 elements to avoid having to fetch all matching elements in the
  // common case where we just want one match.

  if (current_matches_.size() > 0) {
    object_id_out = current_matches_[0];
    current_matches_.clear();
    current_match_arrays_.clear();
    return true;
  }
  if (!current_match_arrays_.empty()) {
    std::string array_object_id = current_match_arrays_[0];
    current_match_arrays_.clear();
    ResolveMatchArrays({array_object_id}, /* max_count= */ 1);
    return false;  // Caller should call again to check
  }
  SendResult(ClientStatus(ELEMENT_RESOLUTION_FAILED));
  return false;
}

bool ElementFinder::ConsumeAllMatchesOrFail(
    std::vector<std::string>& matches_out) {
  if (!current_match_arrays_.empty()) {
    std::vector<std::string> array_object_ids =
        std::move(current_match_arrays_);
    ResolveMatchArrays(array_object_ids, /* max_count= */ -1);
    return false;  // Caller should call again to check
  }
  if (!current_matches_.empty()) {
    matches_out = std::move(current_matches_);
    current_matches_.clear();
    return true;
  }
  SendResult(ClientStatus(ELEMENT_RESOLUTION_FAILED));
  return false;
}

bool ElementFinder::ConsumeMatchArrayOrFail(std::string& array_object_id) {
  if (current_matches_.empty() && current_match_arrays_.empty()) {
    SendResult(ClientStatus(ELEMENT_RESOLUTION_FAILED));
    return false;
  }

  if (current_matches_.empty() && current_match_arrays_.size() == 1) {
    array_object_id = current_match_arrays_[0];
    current_match_arrays_.clear();
    return true;
  }

  std::vector<std::unique_ptr<runtime::CallArgument>> arguments;
  std::string object_id;  // Will be "this" in Javascript.
  std::string function;
  if (current_match_arrays_.size() > 1) {
    object_id = current_match_arrays_.back();
    current_match_arrays_.pop_back();
    // Merge both arrays into current_match_arrays_[0]
    function = "function(dest) { dest.push(...this); }";
    AddRuntimeCallArgumentObjectId(current_match_arrays_[0], &arguments);
  } else if (!current_matches_.empty()) {
    object_id = current_matches_.back();
    current_matches_.pop_back();
    if (current_match_arrays_.empty()) {
      // Create an array containing a single element.
      function = "function() { return [this]; }";
    } else {
      // Add an element to an existing array.
      function = "function(dest) { dest.push(this); }";
      AddRuntimeCallArgumentObjectId(current_match_arrays_[0], &arguments);
    }
  }
  devtools_client_->GetRuntime()->CallFunctionOn(
      runtime::CallFunctionOnParams::Builder()
          .SetObjectId(object_id)
          .SetArguments(std::move(arguments))
          .SetFunctionDeclaration(function)
          .Build(),
      current_frame_id_,
      base::BindOnce(&ElementFinder::OnConsumeMatchArray,
                     weak_ptr_factory_.GetWeakPtr()));
  return false;
}

void ElementFinder::OnConsumeMatchArray(
    const DevtoolsClient::ReplyStatus& reply_status,
    std::unique_ptr<runtime::CallFunctionOnResult> result) {
  ClientStatus status =
      CheckJavaScriptResult(reply_status, result.get(), __FILE__, __LINE__);
  if (!status.ok()) {
    VLOG(1) << __func__ << ": Failed to get element from array for "
            << selector_;
    SendResult(status);
    return;
  }
  if (current_match_arrays_.empty()) {
    std::string returned_object_id;
    if (SafeGetObjectId(result->GetResult(), &returned_object_id)) {
      current_match_arrays_.push_back(returned_object_id);
    }
  }
  ExecuteNextTask();
}

void ElementFinder::GetDocumentElement() {
  devtools_client_->GetRuntime()->Evaluate(
      std::string(kGetDocumentElement), current_frame_id_,
      base::BindOnce(&ElementFinder::OnGetDocumentElement,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ElementFinder::OnGetDocumentElement(
    const DevtoolsClient::ReplyStatus& reply_status,
    std::unique_ptr<runtime::EvaluateResult> result) {
  ClientStatus status =
      CheckJavaScriptResult(reply_status, result.get(), __FILE__, __LINE__);
  if (!status.ok()) {
    VLOG(1) << __func__ << " Failed to get document root element.";
    SendResult(status);
    return;
  }
  std::string object_id;
  if (!SafeGetObjectId(result->GetResult(), &object_id)) {
    VLOG(1) << __func__ << " Failed to get document root element.";
    SendResult(ClientStatus(ELEMENT_RESOLUTION_FAILED));
    return;
  }

  current_frame_root_ = object_id;
  // Use the node as root for the rest of the evaluation.
  current_matches_.emplace_back(object_id);

  DecrementResponseCountAndContinue();
}

void ElementFinder::ApplyJsFilters(const JsFilterBuilder& builder,
                                   const std::vector<std::string>& object_ids) {
  DCHECK(!object_ids.empty());  // Guaranteed by ExecuteNextTask()
  pending_response_count_ = object_ids.size();
  std::string function = builder.BuildFunction();
  for (const std::string& object_id : object_ids) {
    devtools_client_->GetRuntime()->CallFunctionOn(
        runtime::CallFunctionOnParams::Builder()
            .SetObjectId(object_id)
            .SetArguments(builder.BuildArgumentList())
            .SetFunctionDeclaration(function)
            .Build(),
        current_frame_id_,
        base::BindOnce(&ElementFinder::OnApplyJsFilters,
                       weak_ptr_factory_.GetWeakPtr()));
  }
}

void ElementFinder::OnApplyJsFilters(
    const DevtoolsClient::ReplyStatus& reply_status,
    std::unique_ptr<runtime::CallFunctionOnResult> result) {
  if (!result) {
    // It is possible for a document element to already exist, but not be
    // available yet to query because the document hasn't been loaded. This
    // results in OnQuerySelectorAll getting a nullptr result. For this specific
    // call, it is expected.
    VLOG(1) << __func__ << ": Context doesn't exist yet to query frame "
            << frame_stack_.size() << " of " << selector_;
    SendResult(ClientStatus(ELEMENT_RESOLUTION_FAILED));
    return;
  }
  ClientStatus status =
      CheckJavaScriptResult(reply_status, result.get(), __FILE__, __LINE__);
  if (!status.ok()) {
    VLOG(1) << __func__ << ": Failed to query selector for frame "
            << frame_stack_.size() << " of " << selector_ << ": " << status;
    SendResult(status);
    return;
  }

  // The result can be empty (nothing found), a an array (multiple matches
  // found) or a single node.
  std::string object_id;
  if (SafeGetObjectId(result->GetResult(), &object_id)) {
    if (result->GetResult()->HasSubtype() &&
        result->GetResult()->GetSubtype() ==
            runtime::RemoteObjectSubtype::ARRAY) {
      current_match_arrays_.emplace_back(object_id);
    } else {
      current_matches_.emplace_back(object_id);
    }
  }
  DecrementResponseCountAndContinue();
}

void ElementFinder::ResolvePseudoElement(
    PseudoType proto_pseudo_type,
    const std::vector<std::string>& object_ids) {
  dom::PseudoType pseudo_type;
  if (!ConvertPseudoType(proto_pseudo_type, &pseudo_type)) {
    VLOG(1) << __func__ << ": Unsupported pseudo-type "
            << PseudoTypeName(proto_pseudo_type);
    SendResult(ClientStatus(INVALID_ACTION));
    return;
  }

  DCHECK(!object_ids.empty());  // Guaranteed by ExecuteNextTask()
  pending_response_count_ = object_ids.size();
  for (const std::string& object_id : object_ids) {
    devtools_client_->GetDOM()->DescribeNode(
        dom::DescribeNodeParams::Builder().SetObjectId(object_id).Build(),
        current_frame_id_,
        base::BindOnce(&ElementFinder::OnDescribeNodeForPseudoElement,
                       weak_ptr_factory_.GetWeakPtr(), pseudo_type));
  }
}

void ElementFinder::OnDescribeNodeForPseudoElement(
    dom::PseudoType pseudo_type,
    const DevtoolsClient::ReplyStatus& reply_status,
    std::unique_ptr<dom::DescribeNodeResult> result) {
  if (!result || !result->GetNode()) {
    VLOG(1) << __func__ << " Failed to describe the node for pseudo element.";
    SendResult(UnexpectedDevtoolsErrorStatus(reply_status, __FILE__, __LINE__));
    return;
  }

  auto* node = result->GetNode();
  if (node->HasPseudoElements()) {
    for (const auto& pseudo_element : *(node->GetPseudoElements())) {
      if (pseudo_element->HasPseudoType() &&
          pseudo_element->GetPseudoType() == pseudo_type) {
        devtools_client_->GetDOM()->ResolveNode(
            dom::ResolveNodeParams::Builder()
                .SetBackendNodeId(pseudo_element->GetBackendNodeId())
                .Build(),
            current_frame_id_,
            base::BindOnce(&ElementFinder::OnResolveNodeForPseudoElement,
                           weak_ptr_factory_.GetWeakPtr()));
        return;
      }
    }
  }
  DecrementResponseCountAndContinue();
}

void ElementFinder::OnResolveNodeForPseudoElement(
    const DevtoolsClient::ReplyStatus& reply_status,
    std::unique_ptr<dom::ResolveNodeResult> result) {
  if (result && result->GetObject() && result->GetObject()->HasObjectId()) {
    current_matches_.emplace_back(result->GetObject()->GetObjectId());
  }
  DecrementResponseCountAndContinue();
}

void ElementFinder::EnterFrame(const std::string& object_id) {
  devtools_client_->GetDOM()->DescribeNode(
      dom::DescribeNodeParams::Builder().SetObjectId(object_id).Build(),
      current_frame_id_,
      base::BindOnce(&ElementFinder::OnDescribeNodeForFrame,
                     weak_ptr_factory_.GetWeakPtr(), object_id));
}

void ElementFinder::OnDescribeNodeForFrame(
    const std::string& object_id,
    const DevtoolsClient::ReplyStatus& reply_status,
    std::unique_ptr<dom::DescribeNodeResult> result) {
  if (!result || !result->GetNode()) {
    VLOG(1) << __func__ << " Failed to describe the node.";
    SendResult(UnexpectedDevtoolsErrorStatus(reply_status, __FILE__, __LINE__));
    return;
  }

  auto* node = result->GetNode();
  std::vector<int> backend_ids;

  if (node->GetNodeName() == "IFRAME") {
    DCHECK(node->HasFrameId());  // Ensure all frames have an id.

    frame_stack_.push_back(BuildResult(object_id));

    auto* frame = FindCorrespondingRenderFrameHost(node->GetFrameId());
    if (!frame) {
      VLOG(1) << __func__ << " Failed to find corresponding owner frame.";
      SendResult(ClientStatus(FRAME_HOST_NOT_FOUND));
      return;
    }
    current_frame_ = frame;
    current_frame_root_.clear();

    if (node->HasContentDocument()) {
      // If the frame has a ContentDocument it's considered a local frame. In
      // this case, current_frame_ doesn't change and can directly use the
      // content document as root for the evaluation.
      backend_ids.emplace_back(node->GetContentDocument()->GetBackendNodeId());
    } else {
      current_frame_id_ = node->GetFrameId();
      // Kick off another find element chain to walk down the OOP iFrame.
      GetDocumentElement();
      return;
    }
  }

  if (node->HasShadowRoots()) {
    // TODO(crbug.com/806868): Support multiple shadow roots.
    backend_ids.emplace_back(
        node->GetShadowRoots()->front()->GetBackendNodeId());
  }

  if (!backend_ids.empty()) {
    devtools_client_->GetDOM()->ResolveNode(
        dom::ResolveNodeParams::Builder()
            .SetBackendNodeId(backend_ids[0])
            .Build(),
        current_frame_id_,
        base::BindOnce(&ElementFinder::OnResolveNode,
                       weak_ptr_factory_.GetWeakPtr()));
    return;
  }

  // Element was not a frame and didn't have shadow dom. This is unexpected, but
  // to remain backward compatible, don't complain and just continue filtering
  // with the current element as root.
  current_matches_.emplace_back(object_id);
  DecrementResponseCountAndContinue();
}

void ElementFinder::OnResolveNode(
    const DevtoolsClient::ReplyStatus& reply_status,
    std::unique_ptr<dom::ResolveNodeResult> result) {
  if (!result || !result->GetObject() || !result->GetObject()->HasObjectId()) {
    VLOG(1) << __func__ << " Failed to resolve object id from backend id.";
    SendResult(UnexpectedDevtoolsErrorStatus(reply_status, __FILE__, __LINE__));
    return;
  }

  std::string object_id = result->GetObject()->GetObjectId();
  if (current_frame_root_.empty()) {
    current_frame_root_ = object_id;
  }
  // Use the node as root for the rest of the evaluation.
  current_matches_.emplace_back(object_id);
  DecrementResponseCountAndContinue();
}

content::RenderFrameHost* ElementFinder::FindCorrespondingRenderFrameHost(
    std::string frame_id) {
  for (auto* frame : web_contents_->GetAllFrames()) {
    if (frame->GetDevToolsFrameToken().ToString() == frame_id) {
      return frame;
    }
  }

  return nullptr;
}

void ElementFinder::ApplyProximityFilter(int filter_index,
                                         const std::string& array_object_id) {
  Selector target_selector;
  target_selector.proto.mutable_filters()->MergeFrom(
      selector_.proto.filters(filter_index).closest().target());
  proximity_target_filter_ =
      std::make_unique<ElementFinder>(web_contents_, devtools_client_,
                                      target_selector, ResultType::kMatchArray);
  proximity_target_filter_->StartInternal(
      base::BindOnce(&ElementFinder::OnProximityFilterTarget,
                     weak_ptr_factory_.GetWeakPtr(), filter_index,
                     array_object_id),
      current_frame_, current_frame_id_, current_frame_root_);
}

void ElementFinder::OnProximityFilterTarget(int filter_index,
                                            const std::string& array_object_id,
                                            const ClientStatus& status,
                                            std::unique_ptr<Result> result) {
  if (!status.ok()) {
    VLOG(1) << __func__
            << " Could not find proximity filter target for resolving "
            << selector_.proto.filters(filter_index);
    SendResult(status);
    return;
  }
  if (result->container_frame_host != current_frame_) {
    VLOG(1) << __func__ << " Cannot compare elements on different frames.";
    SendResult(ClientStatus(INVALID_SELECTOR));
    return;
  }

  const auto& filter = selector_.proto.filters(filter_index).closest();

  std::string function = R"(function(targets, maxPairs) {
  const candidates = this;
  const pairs = candidates.length * targets.length;
  if (pairs > maxPairs) {
    return pairs;
  }
  const candidateBoxes = candidates.map((e) => e.getBoundingClientRect());
  let closest = null;
  let shortestDistance = Number.POSITIVE_INFINITY;
  for (target of targets) {
    const targetBox = target.getBoundingClientRect();
    for (let i = 0; i < candidates.length; i++) {
      const box = candidateBoxes[i];
)";

  if (filter.in_alignment()) {
    // Rejects candidates that are not on the same row or or the same column as
    // the target.
    function.append("if ((box.bottom <= targetBox.top || ");
    function.append("     box.top >= targetBox.bottom) && ");
    function.append("    (box.right <= targetBox.left || ");
    function.append("     box.left >= targetBox.right)) continue;");
  }
  switch (filter.relative_position()) {
    case SelectorProto::ProximityFilter::UNSPECIFIED_POSITION:
      // No constraints.
      break;

    case SelectorProto::ProximityFilter::ABOVE:
      // Candidate must be above target
      function.append("if (box.bottom > targetBox.top) continue;");
      break;

    case SelectorProto::ProximityFilter::BELOW:
      // Candidate must be below target
      function.append("if (box.top < targetBox.bottom) continue;");
      break;

    case SelectorProto::ProximityFilter::LEFT:
      // Candidate must be left of target
      function.append("if (box.right > targetBox.left) continue;");
      break;

    case SelectorProto::ProximityFilter::RIGHT:
      // Candidate must be right of target
      function.append("if (box.left < targetBox.right) continue;");
      break;
  }

  // The algorithm below computes distance to the closest border. If the
  // distance is 0, then we have got our closest element and can stop there.
  function.append(R"(
      let w = 0;
      if (targetBox.right < box.left) {
        w = box.left - targetBox.right;
      } else if (box.right < targetBox.left) {
        w = targetBox.left - box.right;
      }
      let h = 0;
      if (targetBox.bottom < box.top) {
        h = box.top - targetBox.bottom;
      } else if (box.bottom < targetBox.top) {
        h = targetBox.top - box.bottom;
      }
      const dist = Math.sqrt(h * h + w * w);
      if (dist == 0) return candidates[i];
      if (dist < shortestDistance) {
        closest = candidates[i];
        shortestDistance = dist;
      }
    }
  }
  return closest;
})");

  std::vector<std::unique_ptr<runtime::CallArgument>> arguments;
  AddRuntimeCallArgumentObjectId(result->object_id, &arguments);
  AddRuntimeCallArgument(filter.max_pairs(), &arguments);

  devtools_client_->GetRuntime()->CallFunctionOn(
      runtime::CallFunctionOnParams::Builder()
          .SetObjectId(array_object_id)
          .SetArguments(std::move(arguments))
          .SetFunctionDeclaration(function)
          .Build(),
      current_frame_id_,
      base::BindOnce(&ElementFinder::OnProximityFilterJs,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ElementFinder::OnProximityFilterJs(
    const DevtoolsClient::ReplyStatus& reply_status,
    std::unique_ptr<runtime::CallFunctionOnResult> result) {
  ClientStatus status =
      CheckJavaScriptResult(reply_status, result.get(), __FILE__, __LINE__);
  if (!status.ok()) {
    VLOG(1) << __func__ << ": Failed to execute proximity filter " << status;
    SendResult(status);
    return;
  }

  std::string object_id;
  if (SafeGetObjectId(result->GetResult(), &object_id)) {
    // Function found a match.
    current_matches_.push_back(object_id);
    ExecuteNextTask();
    return;
  }

  int pair_count = 0;
  if (SafeGetIntValue(result->GetResult(), &pair_count)) {
    // Function got too many pairs to check.
    VLOG(1) << __func__ << ": Too many pairs to consider for proximity checks: "
            << pair_count;
    SendResult(ClientStatus(TOO_MANY_CANDIDATES));
    return;
  }

  // Function found nothing, which is possible if the relative position
  // constraints forced the algorithm to discard all candidates.
  ExecuteNextTask();
}

void ElementFinder::ResolveMatchArrays(
    const std::vector<std::string>& array_object_ids,
    int max_count) {
  if (array_object_ids.empty()) {
    // Nothing to do
    ExecuteNextTask();
    return;
  }
  pending_response_count_ = array_object_ids.size();
  for (const std::string& array_object_id : array_object_ids) {
    ResolveMatchArrayRecursive(array_object_id, 0, max_count);
  }
}

void ElementFinder::ResolveMatchArrayRecursive(
    const std::string& array_object_id,
    int index,
    int max_count) {
  std::vector<std::unique_ptr<runtime::CallArgument>> arguments;
  AddRuntimeCallArgument(index, &arguments);
  devtools_client_->GetRuntime()->CallFunctionOn(
      runtime::CallFunctionOnParams::Builder()
          .SetObjectId(array_object_id)
          .SetArguments(std::move(arguments))
          .SetFunctionDeclaration(std::string(kGetArrayElement))
          .Build(),
      current_frame_id_,
      base::BindOnce(&ElementFinder::OnResolveMatchArray,
                     weak_ptr_factory_.GetWeakPtr(), array_object_id, index,
                     max_count));
}

void ElementFinder::OnResolveMatchArray(
    const std::string& array_object_id,
    int index,
    int max_count,
    const DevtoolsClient::ReplyStatus& reply_status,
    std::unique_ptr<runtime::CallFunctionOnResult> result) {
  ClientStatus status =
      CheckJavaScriptResult(reply_status, result.get(), __FILE__, __LINE__);
  if (!status.ok()) {
    VLOG(1) << __func__ << ": Failed to get element from array for "
            << selector_;
    SendResult(status);
    return;
  }
  std::string object_id;
  if (!SafeGetObjectId(result->GetResult(), &object_id)) {
    // We've reached the end of the array
    DecrementResponseCountAndContinue();
    return;
  }

  current_matches_.emplace_back(object_id);
  int next_index = index + 1;
  if (max_count != -1 && next_index >= max_count) {
    DecrementResponseCountAndContinue();
    return;
  }

  // Fetch the next element.
  ResolveMatchArrayRecursive(array_object_id, next_index, max_count);
}

void ElementFinder::DecrementResponseCountAndContinue() {
  if (pending_response_count_ > 1) {
    pending_response_count_--;
    return;
  }

  pending_response_count_ = 0;
  ExecuteNextTask();
  return;
}

}  // namespace autofill_assistant
