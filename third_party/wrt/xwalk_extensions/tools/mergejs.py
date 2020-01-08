#!/usr/bin/env python

# Copyright (c) 2014 Samsung Electronics Co., Ltd. All rights reserved.

import fileinput
import sys
import getopt
import glob
import os

class Utils:
    reqfiles = []
    searchfile = '*_api.js'
    startwith = "//= require('"
    endwith = "')"
    code = ""

    @classmethod
    def get_require(self, s):
        try:
            start = s.index(self.startwith) + len(self.startwith)
            end = s.index(self.endwith, start)
            filename = s[start:end]
            self.reqfiles.append(filename)
        except ValueError:
           return ""

    @classmethod
    def find_require(self):
        p = os.path.join('./', self.searchfile)
        filenames = glob.glob(self.searchfile)
        for fname in filenames:
            with open(fname, 'r') as myfile:
                for line in myfile:
                    self.get_require(line)

    @classmethod
    def print_lines(self, filename):
        with open(filename, 'r') as file:
            for line in file:
                self.code += line

    @classmethod
    def merge_js_files(self, path):
        self.find_require()
        if len(self.reqfiles) == 0:
            s = os.path.join('./', self.searchfile)
            sfiles = glob.glob(s)
            for fname in sfiles:
                self.print_lines(fname)
        else:
            js = '*.js'
            p = os.path.join(path, js)
            filenames = glob.glob(p)
            for fname in self.reqfiles:
                fname = path + '/' + fname
                if fname in filenames:
                    self.print_lines(fname)

    @classmethod
    def main(self, argv):
        path = 'js'
        try:
            opts, args = getopt.getopt(argv,"hf:p:",["file=", "path="])
        except getopt.GetoptError:
            print __file__ + ' -h'
            sys.exit()
        if len(argv) > 0:
          for opt, arg in opts:
              if opt in ("-h"):
                  print 'Help:'
                  print ''
                  print __file__ + '-f <file> -p <path>'
                  print ''
                  print '<opt> \t <opt> \t\t <description>'
                  print '-f \t --file \t Name of the file where script searching for require files:'
                  print '\t \t \t ' + self.startwith + 'file_name.js' + self.endwith
                  print '-p \t --path \t Path to "' + path + '" directory'
                  print ''
                  sys.exit()
              elif opt in ("-f", "--file"):
                  self.searchfile = arg
              elif opt in ("-p", "--path"):
                  path = arg
        self.merge_js_files(path)
        print self.code

if Utils.__module__ == "__main__":
    Utils.main(sys.argv[1:])
