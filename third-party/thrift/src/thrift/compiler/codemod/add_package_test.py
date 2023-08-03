# Copyright (c) Meta Platforms, Inc. and affiliates.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

import os
import shutil
import tempfile
import textwrap
import unittest

import pkg_resources

from thrift.compiler.codemod.test_utils import read_file, run_binary, write_file


class ThriftPackage(unittest.TestCase):
    def setUp(self):
        tmp = tempfile.mkdtemp()
        self.addCleanup(shutil.rmtree, tmp, True)
        self.tmp = tmp
        self.addCleanup(os.chdir, os.getcwd())
        os.chdir(self.tmp)
        self.maxDiff = None

    def trim(self, s):
        return "\n".join([line.strip() for line in s.splitlines()])

    def write_and_test(self, file, content, modified_content):
        write_file(file, textwrap.dedent(content))

        binary = pkg_resources.resource_filename(__name__, "codemod")
        run_binary(binary, file)

        self.assertEqual(
            self.trim(read_file(file)),
            self.trim(modified_content),
        )

    def test_existing_package(self):
        self.write_and_test(
            "foo.thrift",
            """\
                package "meta.com/thrift/annotation"

                struct Bar {}

                """,
            """\
                package "meta.com/thrift/annotation"

                struct Bar {}
                """,
        )

    def test_package_from_file_path(self):
        self.write_and_test(
            "fbcode/thrift/test/foo.thrift",
            """\
                /*
                 *  **License docblock**
                 */

                struct S {
                }

                """,
            """\
                /*
                 *  **License docblock**
                 */

                package "meta.com/thrift/test/foo"

                namespace cpp2 "cpp2"
                namespace hack ""

                struct S {
                }
                """,
        )
