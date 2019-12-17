#!/usr/bin/env python

# Copyright (c) 2013 Intel Corporation. All rights reserved.
# Copyright (c) 2014 Samsung Electronics Co., Ltd. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys
import subprocess

TEMPLATE = """\
extern const char %s[];
const char %s[] = { %s, 0 };
"""

js_code = sys.argv[1]
cmd = "python " + os.path.dirname(__file__) + "/mergejs.py -f" + js_code
lines = subprocess.check_output(cmd, shell=True)
c_code = ', '.join(str(ord(c)) for c in lines)

symbol_name = sys.argv[2]
output = open(sys.argv[3], "w")
output.write(TEMPLATE % (symbol_name, symbol_name, c_code))
output.close()
