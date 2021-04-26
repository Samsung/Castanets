# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""GPU implementation of //testing/skia_gold_common/skia_gold_session.py."""

import os
import subprocess
import tempfile

from gpu_tests import path_util
path_util.AddDirToPathIfNeeded(path_util.GetChromiumSrcDir(), 'build')
from skia_gold_common import skia_gold_session


class GpuSkiaGoldSession(skia_gold_session.SkiaGoldSession):
  def RunComparison(self, name, png_file, output_manager=True, use_luci=True):
    # Passing True for the output manager is a bit of a hack, as we don't
    # actually need an output manager and just need to get past the truthy
    # check.
    return super(GpuSkiaGoldSession,
                 self).RunComparison(name=name,
                                     png_file=png_file,
                                     output_manager=output_manager,
                                     use_luci=use_luci)

  def _CreateDiffOutputDir(self):
    # We intentionally don't clean this up and don't put it in self._working_dir
    # since we need it to stick around after the test completes so the user
    # can look at its contents.
    return tempfile.mkdtemp()

  def _StoreDiffLinks(self, image_name, _, output_dir):
    results = self._comparison_results.setdefault(image_name,
                                                  self.ComparisonResults())
    # The directory should contain "input-<hash>.png", "closest-<hash>.png",
    # and "diff.png".
    for f in os.listdir(output_dir):
      file_url = 'file://%s' % os.path.join(output_dir, f)
      if f.startswith('input-'):
        results.local_diff_given_image = file_url
      elif f.startswith('closest-'):
        results.local_diff_closest_image = file_url
      elif f == 'diff.png':
        results.local_diff_diff_image = file_url

  @staticmethod
  def _RunCmdForRcAndOutput(cmd):
    try:
      output = subprocess.check_output(cmd, stderr=subprocess.STDOUT)
      return 0, output
    except subprocess.CalledProcessError as e:
      return e.returncode, e.output
