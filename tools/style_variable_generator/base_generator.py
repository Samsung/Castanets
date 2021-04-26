# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys
import collections
import re
import textwrap
import path_overrides
from color import Color

_FILE_PATH = os.path.dirname(os.path.realpath(__file__))

_JSON5_PATH = os.path.join(_FILE_PATH, os.pardir, os.pardir, 'third_party',
                           'pyjson5', 'src')
sys.path.insert(1, _JSON5_PATH)
import json5

_JINJA2_PATH = os.path.join(_FILE_PATH, os.pardir, os.pardir, 'third_party')
sys.path.insert(1, _JINJA2_PATH)
import jinja2


class Modes:
    LIGHT = 'light'
    DARK = 'dark'
    ALL = [LIGHT, DARK]


class VariableType:
    COLOR = 'color'
    OPACITY = 'opacity'


class ModeVariables:
    '''A dictionary of variable names to their values in each mode.
       e.g mode_variables['blue'][Modes.LIGHT] = Color(...)
    '''

    def __init__(self, default_mode):
        self.variables = collections.OrderedDict()
        self._default_mode = default_mode

    def Add(self, mode, name, value):
        if name not in self.variables:
            self.variables[name] = {}
        self.variables[name][mode] = value

    # Returns the value that |name| will have in |mode|. Resolves to the default
    # mode's value if the a value for |mode| isn't specified. Always returns a
    # value.
    def Resolve(self, name, mode):
        if mode in self.variables[name]:
            return self.variables[name][mode]

        return self.variables[name][self._default_mode]

    def keys(self):
        return self.variables.keys()

    def items(self):
        return self.variables.items()

    def __getitem__(self, key):
        return self.variables[key]


class BaseGenerator:
    '''A generic style variable generator.

    Subclasses should provide format-specific generation templates, filters and
    globals to render their output.
    '''

    def __init__(self):
        self.out_file_path = None
        self.in_files = []
        self.generate_single_mode = None

        # The mode that colors will fallback to when not specified in a
        # non-default mode. An error will be raised if a color in any mode is
        # not specified in the default mode.
        self._default_mode = Modes.LIGHT

        # A dictionary of |VariableType| to dictionaries of variable names to
        # values. May point to a ModeVariables instance which further adds a
        # layer making the structure name -> mode -> value.
        self.model = {
            VariableType.COLOR: ModeVariables(self._default_mode),
            VariableType.OPACITY: ModeVariables(self._default_mode),
        }


    def AddColor(self, name, value_obj):
        try:
            if isinstance(value_obj, unicode):
                self.model[VariableType.COLOR].Add(self._default_mode, name,
                                                   Color(value_obj))
            elif isinstance(value_obj, dict):
                for mode in Modes.ALL:
                    if mode in value_obj:
                        self.model[VariableType.COLOR].Add(
                            mode, name, Color(value_obj[mode]))
        except ValueError as err:
            raise ValueError('Error parsing color "%s": %s' % (value_obj, err))

    def AddJSONFileToModel(self, path):
        self.in_files.append(path)
        with open(path, 'r') as f:
            self.AddJSONToModel(f.read())

    def AddJSONToModel(self, json_string):
        # TODO(calamity): Add allow_duplicate_keys=False once pyjson5 is
        # rolled.
        data = json5.loads(json_string,
                           object_pairs_hook=collections.OrderedDict)

        try:
            for name, value in data['colors'].items():
                if not re.match('^[a-z0-9_]+$', name):
                    raise ValueError(
                        '%s is not a valid variable name (lower case, 0-9, _)'
                        % name)

                self.AddColor(name, value)
        except ValueError as err:
            raise ValueError('\n%s:\n    %s' % (path, err))

    def ApplyTemplate(self, style_generator, path_to_template, params):
        loader_root_dir = path_overrides.GetFileSystemLoaderRootDirectory()
        jinja_env = jinja2.Environment(
            loader=jinja2.FileSystemLoader(loader_root_dir),
            keep_trailing_newline=True)
        jinja_env.globals.update(style_generator.GetGlobals())
        jinja_env.filters.update(style_generator.GetFilters())
        template = jinja_env.get_template(
            path_overrides.GetPathToTemplate(path_to_template))
        return template.render(params)

    def Validate(self):
        colors = self.model[VariableType.COLOR]

        def CheckColorInDefaultMode(name):
            if (name not in colors.variables
                    or self._default_mode not in colors.variables[name]):
                raise ValueError("%s not defined in default mode '%s'" %
                                 (name, self._default_mode))

        # Check all colors in all modes refer to colors that exist in the
        # default mode.
        for name, mode_values in colors.variables.items():
            for mode, value in mode_values.items():
                CheckColorInDefaultMode(name)
                if value.var:
                    CheckColorInDefaultMode(value.var)
                if value.rgb_var:
                    CheckColorInDefaultMode(value.rgb_var[:-4])

        # TODO(calamity): Check for circular references.

    # TODO(crbug.com/1053372): Prune unused rgb values.
