#!/usr/bin/env vpython
# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

#pylint: disable=protected-access

import os
import tempfile
import unittest

import mock

from gpu_tests import path_util
from gpu_tests.skia_gold import gpu_skia_gold_properties
from gpu_tests.skia_gold import gpu_skia_gold_session

from pyfakefs import fake_filesystem_unittest

path_util.AddDirToPathIfNeeded(path_util.GetChromiumSrcDir(), 'build')
from skia_gold_common import unittest_utils

createSkiaGoldArgs = unittest_utils.createSkiaGoldArgs


def assertArgWith(test, arg_list, arg, value):
  i = arg_list.index(arg)
  test.assertEqual(arg_list[i + 1], value)


class GpuSkiaGoldSessionDiffTest(fake_filesystem_unittest.TestCase):
  def setUp(self):
    self.setUpPyfakefs()
    self._working_dir = tempfile.mkdtemp()

  @mock.patch.object(gpu_skia_gold_session.GpuSkiaGoldSession,
                     '_RunCmdForRcAndOutput')
  def test_commandCommonArgs(self, cmd_mock):
    cmd_mock.return_value = (None, None)
    args = createSkiaGoldArgs(git_revision='a', local_pixel_tests=False)
    sgp = gpu_skia_gold_properties.GpuSkiaGoldProperties(args)
    session = gpu_skia_gold_session.GpuSkiaGoldSession(self._working_dir,
                                                       sgp,
                                                       None,
                                                       'corpus',
                                                       instance='instance')
    session.Diff('name', 'png_file', None)
    call_args = cmd_mock.call_args[0][0]
    self.assertIn('diff', call_args)
    assertArgWith(self, call_args, '--corpus', 'corpus')
    assertArgWith(self, call_args, '--instance', 'instance')
    assertArgWith(self, call_args, '--input', 'png_file')
    assertArgWith(self, call_args, '--test', 'name')
    assertArgWith(self, call_args, '--work-dir', self._working_dir)
    i = call_args.index('--out-dir')
    # The output directory should not be a subdirectory of the working
    # directory.
    self.assertNotIn(self._working_dir, call_args[i + 1])


class GpuSkiaGoldSessionStoreDiffLinksTest(fake_filesystem_unittest.TestCase):
  def setUp(self):
    self.setUpPyfakefs()
    self._working_dir = tempfile.mkdtemp()

  def test_outputManagerNotNeeded(self):
    args = createSkiaGoldArgs(git_revision='a', local_pixel_tests=True)
    sgp = gpu_skia_gold_properties.GpuSkiaGoldProperties(args)
    session = gpu_skia_gold_session.GpuSkiaGoldSession(self._working_dir, sgp,
                                                       None, None, None)
    input_filepath = os.path.join(self._working_dir, 'input-inputhash.png')
    with open(input_filepath, 'w') as f:
      f.write('')
    closest_filepath = os.path.join(self._working_dir,
                                    'closest-closesthash.png')
    with open(closest_filepath, 'w') as f:
      f.write('')
    diff_filepath = os.path.join(self._working_dir, 'diff.png')
    with open(diff_filepath, 'w') as f:
      f.write('')

    session._StoreDiffLinks('foo', None, self._working_dir)
    self.assertEqual(session.GetGivenImageLink('foo'),
                     'file://' + input_filepath)
    self.assertEqual(session.GetClosestImageLink('foo'),
                     'file://' + closest_filepath)
    self.assertEqual(session.GetDiffImageLink('foo'), 'file://' + diff_filepath)


if __name__ == '__main__':
  unittest.main(verbosity=2)
