#!/usr/bin/env vpython
# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Runs WPT as an isolate bundle.

This script maps flags supported by run_isolate_script_test.py to flags that are
understood by WPT.

Here's the mapping [isolate script flag] : [wpt flag]
--isolated-script-test-output : --log-chromium
--total-shards : --total-chunks
--shard-index : -- this-chunk
"""

import json
import os
import sys

import common
import wpt_common

WPT_METADATA_DIR = "../../wpt_expectations_metadata/"
WPT_OVERRIDE_EXPECTATIONS_PATH = (
    "../../third_party/blink/web_tests/WPTOverrideExpectations")

class WPTTestAdapter(wpt_common.BaseWptScriptAdapter):

    @property
    def rest_args(self):
        rest_args = super(WPTTestAdapter, self).rest_args
        # Here we add all of the arguments required to run WPT tests on Chrome.
        rest_args.extend([
            "../../third_party/blink/web_tests/external/wpt/wpt",
            "--venv=../../",
            "--skip-venv-setup",
            "run",
            "chrome",
            "--binary=../../out/Release/chrome",
            "--binary-arg=--host-resolver-rules="
                "MAP nonexistent.*.test ~NOTFOUND, MAP *.test 127.0.0.1",
            "--binary-arg=--enable-experimental-web-platform-features",
            "--binary-arg=--enable-blink-features=MojoJS,MojoJSTest",
            "--webdriver-binary=../../out/Release/chromedriver",
            "--headless",
            "--no-capture-stdio",
            "--no-manifest-download",
            "--no-pause-after-test",
            # Exclude webdriver tests for now. They are run separately on the CI
            "--exclude=webdriver",
            "--exclude=infrastructure/webdriver",
            # Setting --no-fail-on-unexpected makes the return code of wpt 0
            # even if there were test failures. The CQ doesn't like this since
            # it uses the exit code to determine which shards to retry (ie:
            # those that had non-zero exit codes).
            #"--no-fail-on-unexpected",
            "--metadata",
            WPT_METADATA_DIR,
            # By specifying metadata above, WPT will try to find manifest in the
            # metadata directory. So here we point it back to the correct path
            # for the manifest.
            # TODO(lpz): Allowing WPT to rebuild its own manifest temporarily to
            # gauge performance impact. Issue with specifying the base manifest
            # below is that it can get stale if tests are renamed, and requires
            # a lengthy import/export cycle to refresh. So we allow WPT to
            # update the manifest in cast it's stale.
            #"--no-manifest-update",
            "--manifest=../../third_party/blink/web_tests/external/"
                "WPT_BASE_MANIFEST_8.json",
            # (crbug.com/1023835) The flags below are temporary to aid debugging
            "--log-mach=-",
            "--log-mach-verbose",
            # See if multi-processing affects timeouts.
            # TODO(lpz): Consider removing --processes and compute automatically
            # from multiprocessing.cpu_count()
            #"--processes=5",
        ])
        return rest_args

def main():
    # First, generate WPT metadata files.
    common.run_command([
        sys.executable,
        os.path.join(wpt_common.BLINK_TOOLS_DIR, 'build_wpt_metadata.py'),
        "--metadata-output-dir",
        WPT_METADATA_DIR,
        "--additional-expectations",
        WPT_OVERRIDE_EXPECTATIONS_PATH
    ])

    adapter = WPTTestAdapter()
    return adapter.run_test()


# This is not really a "script test" so does not need to manually add
# any additional compile targets.
def main_compile_targets(args):
    json.dump([], args.output)


if __name__ == '__main__':
    # Conform minimally to the protocol defined by ScriptTest.
    if 'compile_targets' in sys.argv:
        funcs = {
            'run': None,
            'compile_targets': main_compile_targets,
        }
        sys.exit(common.run_script(sys.argv[1:], funcs))
    sys.exit(main())
