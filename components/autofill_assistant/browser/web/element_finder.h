// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_WEB_ELEMENT_FINDER_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_WEB_ELEMENT_FINDER_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/strcat.h"
#include "components/autofill_assistant/browser/client_status.h"
#include "components/autofill_assistant/browser/devtools/devtools/domains/types_dom.h"
#include "components/autofill_assistant/browser/devtools/devtools/domains/types_runtime.h"
#include "components/autofill_assistant/browser/devtools/devtools_client.h"
#include "components/autofill_assistant/browser/selector.h"
#include "components/autofill_assistant/browser/web/web_controller_worker.h"

namespace content {
class WebContents;
class RenderFrameHost;
}  // namespace content

namespace autofill_assistant {
class DevtoolsClient;

// Worker class to find element(s) matching a selector.
class ElementFinder : public WebControllerWorker {
 public:
  enum ResultType {
    // Result.object_id contains the object ID of the single node that matched.
    // If there are no matches, status is ELEMENT_RESOLUTION_FAILED. If there
    // are more than one matches, status is TOO_MANY_ELEMENTS.
    kExactlyOneMatch = 0,

    // Result.object_id contains the object ID of one of the nodes that matched.
    // If there are no matches, status is ELEMENT_RESOLUTION_FAILED.
    kAnyMatch,

    // Result.object_id contains the object ID of an array containing all the
    // nodes
    // that matched. If there are no matches, status is
    // ELEMENT_RESOLUTION_FAILED.
    kMatchArray,
  };

  struct Result {
    Result();
    ~Result();
    Result(const Result&);

    // The render frame host contains the element.
    content::RenderFrameHost* container_frame_host = nullptr;

    // The object id of the element.
    std::string object_id;

    // The frame id to use to execute devtools Javascript calls within the
    // context of the frame. Might be empty if no frame id needs to be
    // specified.
    std::string node_frame_id;

    std::vector<Result> frame_stack;
  };

  // |web_contents| and |devtools_client| must be valid for the lifetime of the
  // instance.
  ElementFinder(content::WebContents* web_contents,
                DevtoolsClient* devtools_client,
                const Selector& selector,
                ResultType result_type);
  ~ElementFinder() override;

  using Callback =
      base::OnceCallback<void(const ClientStatus&, std::unique_ptr<Result>)>;

  // Finds the element and calls the callback.
  void Start(Callback callback_);

 private:
  // Helper for building JavaScript functions.
  //
  // TODO(b/155264465): extract this into a top-level class in its own file, so
  // it can be tested.
  class JsFilterBuilder {
   public:
    JsFilterBuilder();
    ~JsFilterBuilder();

    // Builds the argument list for the function.
    std::vector<std::unique_ptr<runtime::CallArgument>> BuildArgumentList()
        const;

    // Return the JavaScript function.
    std::string BuildFunction() const;

    // Adds a filter, if possible.
    bool AddFilter(const SelectorProto::Filter& filter);

   private:
    std::vector<std::string> arguments_;
    std::vector<std::string> lines_;

    // A number that's increased by each call to DeclareVariable() to make sure
    // we generate unique variables.
    int variable_counter_ = 0;

    // Adds a regexp filter.
    void AddRegexpFilter(const SelectorProto::TextFilter& filter,
                         const std::string& property);

    // Declares and initializes a variable containing a RegExp object that
    // correspond to |filter| and returns the variable name.
    std::string AddRegexpInstance(const SelectorProto::TextFilter& filter);

    // Returns the name of a new unique variable.
    std::string DeclareVariable();

    // Adds an argument to the argument list and returns its JavaScript
    // representation.
    //
    // This allows passing strings to the JavaScript code without having to
    // hardcode and escape them - this helps avoid XSS issues.
    std::string AddArgument(const std::string& value);

    // Adds a line of JavaScript code to the function, between the header and
    // footer. At that point, the variable "elements" contains the current set
    // of matches, as an array of nodes. It should be updated to contain the new
    // set of matches.
    void AddLine(const std::string& line) { lines_.emplace_back(line); }

    void AddLine(const std::vector<std::string>& line) {
      lines_.emplace_back(base::StrCat(line));
    }
  };

  // Finds the element, starting at |frame| and calls |callback|.
  //
  // |document_object_id| might be empty, in which case we first look for the
  // frame's document.
  void StartInternal(Callback callback,
                     content::RenderFrameHost* frame,
                     const std::string& frame_id,
                     const std::string& document_object_id);

  // Sends a result with the given status and no data.
  void SendResult(const ClientStatus& status);

  // Builds a result from the current state of the finder and returns it.
  void SendSuccessResult(const std::string& object_id);

  // Report |object_id| as result in |result| and initialize the frame-related
  // fields of |result| from the current state. Leaves the frame stack empty.
  Result BuildResult(const std::string& object_id);

  // Figures out what to do next given the current state.
  //
  // Most background operations in this worker end by updating the state and
  // calling ExecuteNextTask() again either directly or through
  // DecrementResponseCountAndContinue().
  void ExecuteNextTask();

  // Make sure there's exactly one match, set it |object_id_out| then return
  // true.
  //
  // If there are too many or too few matches, this function sends an error and
  // returns false.
  //
  // If this returns true, continue processing. If this returns false, return
  // from ExecuteNextTask(). ExecuteNextTask() will be called again once the
  // required data is available.
  bool ConsumeOneMatchOrFail(std::string& object_id_out);

  // Make sure there's at least one match, take one and put it in
  // |object_id_out|, then return true.
  //
  // If there are no matches, send an error response and return false.
  // If there are not enough matches yet, fetch them in the background and
  // return false. This calls ExecuteNextTask() once matches have been fetched.
  //
  // If this returns true, continue processing. If this returns false, return
  // from ExecuteNextTask(). ExecuteNextTask() will be called again once the
  // required data is available.
  bool ConsumeAnyMatchOrFail(std::string& object_id_out);

  // Make sure there's at least one match and move them all into
  // |matches_out|.
  //
  // If there are no matches, send an error response and return false.
  // If there are not enough matches yet, fetch them in the background and
  // return false. This calls ExecuteNextTask() once matches have been fetched.
  //
  // If this returns true, continue processing. If this returns false, return
  // from ExecuteNextTask(). ExecuteNextTask() will be called again once the
  // required data is available.
  bool ConsumeAllMatchesOrFail(std::vector<std::string>& matches_out);

  // Make sure there's at least one match and move them all into a single array.
  //
  // If there are no matches, call SendResult() return false. If there are
  // matches, but they're not in a single array, move the element into the array
  // in the background and return false. ExecuteNextTask() is called again once
  // the background tasks have executed.
  bool ConsumeMatchArrayOrFail(std::string& array_object_id_out);

  void OnConsumeMatchArray(
      const DevtoolsClient::ReplyStatus& reply_status,
      std::unique_ptr<runtime::CallFunctionOnResult> result);

  // Gets a document element from the current frame and us it as root for the
  // rest of the tasks.
  void GetDocumentElement();
  void OnGetDocumentElement(const DevtoolsClient::ReplyStatus& reply_status,
                            std::unique_ptr<runtime::EvaluateResult> result);

  // Handle Javascript filters
  void ApplyJsFilters(const JsFilterBuilder& builder,
                      const std::vector<std::string>& object_ids);
  void OnApplyJsFilters(const DevtoolsClient::ReplyStatus& reply_status,
                        std::unique_ptr<runtime::CallFunctionOnResult> result);

  // Handle PSEUDO_TYPE
  void ResolvePseudoElement(PseudoType pseudo_type,
                            const std::vector<std::string>& object_ids);
  void OnDescribeNodeForPseudoElement(
      dom::PseudoType pseudo_type,
      const DevtoolsClient::ReplyStatus& reply_status,
      std::unique_ptr<dom::DescribeNodeResult> result);
  void OnResolveNodeForPseudoElement(
      const DevtoolsClient::ReplyStatus& reply_status,
      std::unique_ptr<dom::ResolveNodeResult> result);

  // Handle ENTER_FRAME
  void EnterFrame(const std::string& object_id);
  void OnDescribeNodeForFrame(const std::string& object_id,
                              const DevtoolsClient::ReplyStatus& reply_status,
                              std::unique_ptr<dom::DescribeNodeResult> result);
  void OnResolveNode(const DevtoolsClient::ReplyStatus& reply_status,
                     std::unique_ptr<dom::ResolveNodeResult> result);
  content::RenderFrameHost* FindCorrespondingRenderFrameHost(
      std::string frame_id);

  // Handle TaskType::PROXIMITY
  void ApplyProximityFilter(int filter_index,
                            const std::string& array_object_id);
  void OnProximityFilterTarget(int filter_index,
                               const std::string& array_object_id,
                               const ClientStatus& status,
                               std::unique_ptr<Result> result);
  void OnProximityFilterJs(
      const DevtoolsClient::ReplyStatus& reply_status,
      std::unique_ptr<runtime::CallFunctionOnResult> result);

  // Get elements from |array_object_ids|, and put the result into
  // |element_matches_|.
  //
  // This calls ExecuteNextTask() once all the elements of all the arrays are in
  // |element_matches_|. If |max_count| is -1, fetch until the end of the array,
  // otherwise fetch |max_count| elements at most in each array.
  void ResolveMatchArrays(const std::vector<std::string>& array_object_ids,
                          int max_count);

  // ResolveMatchArrayRecursive calls itself recursively, incrementing |index|,
  // as long as there are elements. The chain of calls end with
  // DecrementResponseCountAndContinue() as there can be more than one such
  // chains executing at a time.
  void ResolveMatchArrayRecursive(const std::string& array_object_ids,
                                  int index,
                                  int max_count);

  void OnResolveMatchArray(
      const std::string& array_object_id,
      int index,
      int max_count,
      const DevtoolsClient::ReplyStatus& reply_status,
      std::unique_ptr<runtime::CallFunctionOnResult> result);

  // Tracks pending_response_count_ and call ExecuteNextTask() once the count
  // has reached 0.
  void DecrementResponseCountAndContinue();

  content::WebContents* const web_contents_;
  DevtoolsClient* const devtools_client_;
  const Selector selector_;
  const ResultType result_type_;
  Callback callback_;

  // The index of the next filter to process, in selector_.proto.filters.
  int next_filter_index_ = 0;

  // Pointer to the current frame
  content::RenderFrameHost* current_frame_ = nullptr;

  // The frame id to use to execute devtools Javascript calls within the
  // context of the frame. Might be empty if no frame id needs to be
  // specified.
  std::string current_frame_id_;

  // Object ID of the root of |current_frame_|.
  std::string current_frame_root_;

  // Object IDs of the current set matching elements. Cleared once it's used to
  // query or filter.
  //
  // More matches can be found in |current_match_arrays_|. Use one of the
  // Consume*Match() function to current matches.
  std::vector<std::string> current_matches_;

  // Object ID of arrays of at least 2 matching elements.
  //
  // More matches can be found in |current_matches_|. Use one of the
  // Consume*Match() function to current matches.
  std::vector<std::string> current_match_arrays_;

  // True if current_matches are pseudo-elements.
  bool matching_pseudo_elements_ = false;

  // Number of responses still pending.
  //
  // Before starting several background operations in parallel, set this counter
  // to the number of operations and make sure that
  // DecrementResponseCountAndContinue() is called once the result of the
  // operation has been processed and the state of ElementFinder updated.
  // DecrementResponseCountAndContinue() will then make sure to call
  // ExecuteNextTask() again once this counter has reached 0 to continue the
  // work.
  size_t pending_response_count_ = 0;

  std::vector<Result> frame_stack_;

  // Finder for the target of the current proximity filter.
  std::unique_ptr<ElementFinder> proximity_target_filter_;

  base::WeakPtrFactory<ElementFinder> weak_ptr_factory_{this};
};

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_WEB_ELEMENT_FINDER_H_
