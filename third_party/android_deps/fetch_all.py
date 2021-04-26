#!/usr/bin/env python3

# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""A script used to manage Google Maven dependencies for Chromium.

For each dependency in `build.gradle`:

  - Download the library
  - Download the LICENSE
  - Generate a README.chromium file
  - Generate a GN target in BUILD.gn
  - Generate .info files for AAR libraries
  - Generate CIPD yaml files describing the packages
  - Generate a 'deps' entry in DEPS.
"""

import argparse
import collections
import concurrent.futures
import contextlib
import fnmatch
import logging
import tempfile
import os
import re
import shutil
import subprocess
import zipfile

# Assume this script is stored under third_party/android_deps/
_CHROMIUM_SRC = os.path.normpath(os.path.join(__file__, '..', '..', '..'))

# Location of the android_deps directory from a root checkout.
_ANDROID_DEPS_SUBDIR = os.path.join('third_party', 'android_deps')

# Path to BUILD.gn file under android_deps/
_ANDROID_DEPS_BUILD_GN = os.path.join(_ANDROID_DEPS_SUBDIR, 'BUILD.gn')

# Path to build.gradle file under android_deps/
_ANDROID_DEPS_BUILD_GRADLE = os.path.join(_ANDROID_DEPS_SUBDIR, 'build.gradle')

# Path to custom licenses under android_deps/
_ANDROID_DEPS_LICENSE_SUBDIR = os.path.join(_ANDROID_DEPS_SUBDIR, 'licenses')

# Path to additional_readme_paths.json
_ANDROID_DEPS_ADDITIONAL_README_PATHS = os.path.join(
    _ANDROID_DEPS_SUBDIR, 'additional_readme_paths.json')

# Location of the android_deps libs directory from a root checkout.
_ANDROID_DEPS_LIBS_SUBDIR = os.path.join(_ANDROID_DEPS_SUBDIR, 'libs')

# Location of the buildSrc directory used implement our gradle task.
_GRADLE_BUILDSRC_PATH = os.path.join(_ANDROID_DEPS_SUBDIR, 'buildSrc')

_JAVA_HOME = os.path.join(_CHROMIUM_SRC, 'third_party', 'jdk', 'current')
_JETIFY_PATH = os.path.join(_CHROMIUM_SRC, 'third_party',
                            'jetifier_standalone', 'bin',
                            'jetifier-standalone')
_JETIFY_CONFIG = os.path.join(_CHROMIUM_SRC, 'third_party',
                              'jetifier_standalone', 'config',
                              'ignore_R.config')

# The lis_ of git-controlled files that are checked or updated by this tool.
_UPDATED_GIT_FILES = [
    'DEPS',
    _ANDROID_DEPS_BUILD_GN,
    _ANDROID_DEPS_ADDITIONAL_README_PATHS,
]

# If this file exists in an aar file then it is appended to LICENSE
_THIRD_PARTY_LICENSE_FILENAME = 'third_party_licenses.txt'

# Path to the aar.py script used to generate .info files.
_AAR_PY = os.path.join(_CHROMIUM_SRC, 'build', 'android', 'gyp', 'aar.py')


@contextlib.contextmanager
def BuildDir(dirname=None):
    """Helper function used to manage a build directory.

  Args:
    dirname: Optional build directory path. If not provided, a temporary
      directory will be created and cleaned up on exit.
  Returns:
    A python context manager modelling a directory path. The manager
    removes the directory if necessary on exit.
  """
    delete = False
    if not dirname:
        dirname = tempfile.mkdtemp()
        delete = True
    try:
        yield dirname
    finally:
        if delete:
            shutil.rmtree(dirname)


def RaiseCommandException(args, returncode, output, error):
    """Raise an exception whose message describing a command failure.

  Args:
    args: shell command-line (as passed to subprocess.call())
    returncode: status code.
    error: standard error output.
  Raises:
    a new Exception.
  """
    message = 'Command failed with status {}: {}\n'.format(returncode, args)
    if output:
        message += 'Output:-----------------------------------------\n{}\n' \
            '------------------------------------------------\n'.format(output)
    if error:
        message += 'Error message: ---------------------------------\n{}\n' \
            '------------------------------------------------\n'.format(error)
    raise Exception(message)


def RunCommand(args, print_stdout=False):
    """Run a new shell command.

  This function runs without printing anything.

  Args:
    args: A string or a list of strings for the shell command.
  Raises:
    On failure, raise an Exception that contains the command's arguments,
    return status, and standard output + error merged in a single message.
  """
    logging.debug('Run %s', args)
    stdout = None if print_stdout else subprocess.PIPE
    p = subprocess.Popen(args, stdout=stdout)
    pout, _ = p.communicate()
    if p.returncode != 0:
        RaiseCommandException(args, p.returncode, None, pout)


def RunCommandAndGetOutput(args):
    """Run a new shell command. Return its output. Exception on failure.

  This function runs without printing anything.

  Args:
    args: A string or a list of strings for the shell command.
  Returns:
    The command's output.
  Raises:
    On failure, raise an Exception that contains the command's arguments,
    return status, and standard output, and standard error as separate
    messages.
  """
    logging.debug('Run %s', args)
    p = subprocess.Popen(args, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    pout, perr = p.communicate()
    if p.returncode != 0:
        RaiseCommandException(args, p.returncode, pout, perr)
    return pout


def MakeDirectory(dir_path):
    """Make directory |dir_path| recursively if necessary."""
    if not os.path.isdir(dir_path):
        logging.debug('mkdir [%s]', dir_path)
        os.makedirs(dir_path)


def DeleteDirectory(dir_path):
    """Recursively delete a directory if it exists."""
    if os.path.exists(dir_path):
        logging.debug('rmdir [%s]', dir_path)
        shutil.rmtree(dir_path)


def CopyFileOrDirectory(src_path, dst_path):
    """Copy file or directory |src_path| into |dst_path| exactly."""
    logging.debug('copy [%s -> %s]', src_path, dst_path)
    MakeDirectory(os.path.dirname(dst_path))
    if os.path.isdir(src_path):
        # Copy directory recursively.
        DeleteDirectory(dst_path)
        shutil.copytree(src_path, dst_path)
    else:
        shutil.copy(src_path, dst_path)


def ReadFile(file_path):
    """Read a file, return its content."""
    with open(file_path) as f:
        return f.read()


def ReadFileAsLines(file_path):
    """Read a file as a series of lines."""
    with open(file_path) as f:
        return f.readlines()


def WriteFile(file_path, file_data):
    """Write a file."""
    if isinstance(file_data, str):
        file_data = file_data.encode('utf8')
    MakeDirectory(os.path.dirname(file_path))
    with open(file_path, 'wb') as f:
        f.write(file_data)


def FindInDirectory(directory, filename_filter):
    """Find all files in a directory that matches a given filename filter."""
    files = []
    for root, _dirnames, filenames in os.walk(directory):
        matched_files = fnmatch.filter(filenames, filename_filter)
        files.extend((os.path.join(root, f) for f in matched_files))
    return files


# Named tuple describing a CIPD package.
# - path: Path to cipd.yaml file.
# - name: cipd package name.
# - tag: cipd tag.
CipdPackageInfo = collections.namedtuple('CipdPackageInfo',
                                         ['path', 'name', 'tag'])

# Regular expressions used to extract useful info from cipd.yaml files
# generated by Gradle. See BuildConfigGenerator.groovy:makeCipdYaml()
_RE_CIPD_CREATE = re.compile('cipd create --pkg-def cipd.yaml -tag (\S*)')
_RE_CIPD_PACKAGE = re.compile('package: (\S*)')


def GetCipdPackageInfo(cipd_yaml_path):
    """Returns the CIPD package name corresponding to a given cipd.yaml file.

  Args:
    cipd_yaml_path: Path of input cipd.yaml file.
  Returns:
    A (package_name, package_tag) tuple.
  Raises:
    Exception if the file could not be read.
  """
    package_name = None
    package_tag = None
    for line in ReadFileAsLines(cipd_yaml_path):
        m = _RE_CIPD_PACKAGE.match(line)
        if m:
            package_name = m.group(1)

        m = _RE_CIPD_CREATE.search(line)
        if m:
            package_tag = m.group(1)

    if not package_name or not package_tag:
        raise Exception('Invalid cipd.yaml format: ' + cipd_yaml_path)

    return (package_name, package_tag)


def ParseDeps(root_dir, libs_dir):
    """Parse an android_deps/libs and retrieve package information.

  Args:
    root_dir: Path to a root Chromium or build directory.
  Returns:
    A directory mapping package names to tuples of
    (cipd_yaml_file, package_name, package_tag), where |cipd_yaml_file|
    is the path to the cipd.yaml file, related to |libs_dir|,
    and |package_name| and |package_tag| are the extracted from it.
  """
    result = {}
    libs_dir = os.path.abspath(os.path.join(root_dir, libs_dir))
    for cipd_file in FindInDirectory(libs_dir, 'cipd.yaml'):
        pkg_name, pkg_tag = GetCipdPackageInfo(cipd_file)
        cipd_path = os.path.dirname(cipd_file)
        cipd_path = cipd_path[len(root_dir) + 1:]
        result[pkg_name] = CipdPackageInfo(cipd_path, pkg_name, pkg_tag)

    return result


def PrintPackageList(packages, list_name):
    """Print a list of packages to standard output.

  Args:
    packages: list of package names.
    list_name: a simple word describing the package list (e.g. 'new')
  """
    print('  {} {} packages:'.format(len(packages), list_name))
    print('\n'.join('    - ' + p for p in packages))


def _GenerateCipdUploadCommands(cipd_pkg_infos):
    """Generates a shell command to upload missing packages."""

    def cipd_describe(info):
        pkg_name, pkg_tag = info[1:]
        result = subprocess.call(
            ['cipd', 'describe', pkg_name, '-version', pkg_tag],
            stdout=subprocess.DEVNULL)
        return info, result

    # Re-run the describe step to prevent mistakes if run multiple times.
    TEMPLATE = ('(cd "{0}"; '
                'cipd describe "{1}" -version "{2}" || '
                'cipd create --pkg-def cipd.yaml -tag "{2}")')
    cmds = []
    # max_workers chosen arbitrarily.
    with concurrent.futures.ThreadPoolExecutor(max_workers=80) as executor:
        for info, result in executor.map(cipd_describe, cipd_pkg_infos):
            if result:
                pkg_path, pkg_name, pkg_tag = info
                # pkg_path is implicitly relative to _CHROMIUM_SRC, make it
                # explicit.
                pkg_path = os.path.join(_CHROMIUM_SRC, pkg_path)
                # Now make pkg_path relative to os.curdir.
                pkg_path = os.path.relpath(pkg_path)
                cmds.append(TEMPLATE.format(pkg_path, pkg_name, pkg_tag))
    return cmds


def _CreateAarInfos(aar_files):
    jobs = []

    for aar_file in aar_files:
        aar_dirname = os.path.dirname(aar_file)
        aar_info_name = os.path.basename(aar_dirname) + '.info'
        aar_info_path = os.path.join(aar_dirname, aar_info_name)

        logging.debug('- %s', aar_info_name)
        cmd = [_AAR_PY, 'list', aar_file, '--output', aar_info_path]
        proc = subprocess.Popen(cmd)
        jobs.append((cmd, proc))

    for cmd, proc in jobs:
        if proc.wait():
            raise Exception('Command Failed: {}\n'.format(' '.join(cmd)))


def _JetifyAll(files, libs_dir):
    env = os.environ.copy()
    env['JAVA_HOME'] = _JAVA_HOME
    env['ANDROID_DEPS'] = libs_dir

    # Don't jetify support lib or androidx.
    EXCLUDE = ('android_arch_', 'androidx_', 'com_android_support_',
               'errorprone', 'jetifier')

    jobs = []
    for path in files:
        if any(x in path for x in EXCLUDE):
            continue
        cmd = [_JETIFY_PATH, '-c', _JETIFY_CONFIG, '-i', path, '-o', path]
        # Hide output: "You don't need to run Jetifier."
        proc = subprocess.Popen(cmd,
                                env=env,
                                stdout=subprocess.PIPE,
                                stderr=subprocess.STDOUT,
                                encoding='ascii')
        jobs.append((cmd, proc))

    num_required = 0
    for cmd, proc in jobs:
        output = proc.communicate()[0]
        if proc.returncode:
            raise Exception(
                'Jetify failed for command: {}\nOutput:\n{}'.format(
                    ' '.join(cmd), output))
        if "You don't need to run Jetifier" not in output:
            logging.info('Needed jetify: %s', cmd[-1])
            num_required += 1
    logging.info('Jetify was needed for %d out of %d files', num_required,
                 len(jobs))


def main():
    parser = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument(
        '--build-dir',
        help='Path to build directory (default is temporary directory).')
    parser.add_argument('--git-dir',
                        help='Path to git subdir from chromium-dir.',
                        default='.')
    parser.add_argument('--ignore-licenses',
                        help='Ignores licenses for these deps.',
                        action='store_true')
    parser.add_argument('-v',
                        '--verbose',
                        dest='verbose_count',
                        default=0,
                        action='count',
                        help='Verbose level (multiple times for more)')
    args = parser.parse_args()

    logging.basicConfig(
        level=logging.WARNING - 10 * args.verbose_count,
        format='%(levelname).1s %(relativeCreated)6d %(message)s')
    debug = args.verbose_count >= 2

    abs_git_dir = os.path.normpath(os.path.join(_CHROMIUM_SRC, args.git_dir))

    if not os.path.isdir(abs_git_dir):
        raise Exception('Not a directory: ' + abs_git_dir)

    build_gradle_path = os.path.join(args.git_dir, _ANDROID_DEPS_BUILD_GRADLE)
    build_gradle_abs_path = os.path.join(abs_git_dir,
                                         _ANDROID_DEPS_BUILD_GRADLE)
    # The list of files and dirs that are copied to the build directory by this
    # script. Should not include _UPDATED_GIT_FILES.
    copied_paths = {
        build_gradle_path:
        build_gradle_path,
        _GRADLE_BUILDSRC_PATH:
        os.path.join(args.git_dir, _ANDROID_DEPS_SUBDIR, "buildSrc"),
    }

    if not args.ignore_licenses:
        copied_paths[
            _ANDROID_DEPS_LICENSE_SUBDIR] = _ANDROID_DEPS_LICENSE_SUBDIR

    missing_files = []
    for src_path in copied_paths.keys():
        if not os.path.exists(os.path.join(_CHROMIUM_SRC, src_path)):
            missing_files.append(src_path)
    for git_file in _UPDATED_GIT_FILES:
        if not os.path.exists(os.path.join(abs_git_dir, git_file)):
            missing_files.append(git_file)
    if missing_files:
        raise Exception('Missing files from {}: {}'.format(
            _CHROMIUM_SRC, missing_files))

    # Path to the gradlew script used to run build.gradle.
    gradle_wrapper_path = os.path.join(_CHROMIUM_SRC, 'third_party',
                                       'gradle_wrapper', 'gradlew')

    with BuildDir(args.build_dir) as build_dir:
        logging.info('Using build directory: %s', build_dir)
        for git_file in _UPDATED_GIT_FILES:
            CopyFileOrDirectory(
                os.path.join(abs_git_dir, git_file),
                os.path.join(build_dir, args.git_dir, git_file))

        for path, dest in copied_paths.items():
            CopyFileOrDirectory(os.path.join(_CHROMIUM_SRC, path),
                                os.path.join(build_dir, dest))

        logging.info('Running Gradle.')
        # This gradle command generates the new DEPS and BUILD.gn files, it can
        # also handle special cases.
        # Edit BuildConfigGenerator.groovy#addSpecialTreatment for such cases.
        gradle_cmd = [
            gradle_wrapper_path,
            '-b',
            os.path.join(build_dir, build_gradle_path),
            'setupRepository',
            '--stacktrace',
        ]
        if debug:
            gradle_cmd.append('--debug')
        if args.ignore_licenses:
            gradle_cmd.append('-PskipLicenses=true')

        subprocess.run(gradle_cmd, check=True)

        libs_dir = os.path.join(build_dir, args.git_dir,
                                _ANDROID_DEPS_LIBS_SUBDIR)

        logging.info('# Reformat %s.', _ANDROID_DEPS_BUILD_GN)
        gn_args = [
            'gn', 'format',
            os.path.join(build_dir, args.git_dir, _ANDROID_DEPS_BUILD_GN)
        ]
        RunCommand(gn_args, print_stdout=debug)

        logging.info('# Jetify all libraries.')
        aar_files = FindInDirectory(libs_dir, '*.aar')
        jar_files = FindInDirectory(libs_dir, '*.jar')
        _JetifyAll(aar_files + jar_files, libs_dir)

        logging.info('# Generate Android .aar info files.')
        _CreateAarInfos(aar_files)

        if not args.ignore_licenses:
            logging.info('# Looking for nested license files.')
            for aar_file in aar_files:
                # Play Services .aar files have embedded licenses.
                with zipfile.ZipFile(aar_file) as z:
                    if _THIRD_PARTY_LICENSE_FILENAME in z.namelist():
                        aar_dirname = os.path.dirname(aar_file)
                        license_path = os.path.join(aar_dirname, 'LICENSE')
                        # Make sure to append as we don't want to lose the
                        # existing license.
                        with open(license_path, 'ab') as f:
                            f.write(z.read(_THIRD_PARTY_LICENSE_FILENAME))

        logging.info('# Compare CIPD packages.')
        existing_packages = ParseDeps(abs_git_dir, _ANDROID_DEPS_LIBS_SUBDIR)
        build_packages = ParseDeps(
            build_dir, os.path.join(args.git_dir, _ANDROID_DEPS_LIBS_SUBDIR))

        deleted_packages = []
        updated_packages = []
        for pkg in sorted(existing_packages):
            if pkg not in build_packages:
                deleted_packages.append(pkg)
            else:
                existing_info = existing_packages[pkg]
                build_info = build_packages[pkg]
                if existing_info.tag != build_info.tag:
                    updated_packages.append(pkg)

        new_packages = sorted(set(build_packages) - set(existing_packages))

        # Generate CIPD package upload commands.
        logging.info('Querying %d CIPD packages', len(build_packages))
        cipd_commands = _GenerateCipdUploadCommands(build_packages[pkg]
                                                    for pkg in build_packages)

        # Copy updated DEPS and BUILD.gn to build directory.
        update_cmds = []
        for updated_file in _UPDATED_GIT_FILES:
            CopyFileOrDirectory(
                os.path.join(build_dir, args.git_dir, updated_file),
                os.path.join(abs_git_dir, updated_file))

        # Delete obsolete or updated package directories.
        for pkg in existing_packages.values():
            pkg_path = os.path.join(abs_git_dir, pkg.path)
            DeleteDirectory(pkg_path)

        # Copy new and updated packages from build directory.
        for pkg in build_packages.values():
            pkg_path = pkg.path
            dst_pkg_path = os.path.join(_CHROMIUM_SRC, pkg_path)
            src_pkg_path = os.path.join(build_dir, pkg_path)
            CopyFileOrDirectory(src_pkg_path, dst_pkg_path)

        # Useful for printing timestamp.
        logging.info('All Done.')

        if new_packages:
            PrintPackageList(new_packages, 'new')
        if updated_packages:
            PrintPackageList(updated_packages, 'updated')
        if deleted_packages:
            PrintPackageList(deleted_packages, 'deleted')

        if cipd_commands:
            print('Run the following to upload CIPD packages:')
            print('-------------------- cut here ------------------------')
            print('\n'.join(cipd_commands))
            print('-------------------- cut here ------------------------')
        else:
            print('Done. All packages were already up-to-date on CIPD')


if __name__ == "__main__":
    main()
