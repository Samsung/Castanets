# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

vars = struct(
    is_master = False,
    ref = 'refs/branch-heads/4103',
    ci_bucket = 'ci-m83',
    ci_poller = 'm83-gitiles-trigger',
    main_console_name = 'main-m83',
    main_console_title = 'Chromium M83 Console',
    cq_mirrors_console_name = 'mirrors-m83',
    cq_mirrors_console_title = 'Chromium M83 CQ Mirrors Console',
    try_bucket = 'try-m83',
    try_triggering_projects = [],
    cq_group = 'cq-m83',
    cq_ref_regexp = 'refs/branch-heads/4103',
    main_list_view_name = 'try-m83',
    main_list_view_title = 'Chromium M83 CQ console',
    tree_status_host = None,
)
