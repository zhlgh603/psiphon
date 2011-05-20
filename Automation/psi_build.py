#!/usr/bin/python
#
# Copyright (c) 2011, Psiphon Inc.
# All rights reserved.
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.
#

import subprocess

def build_client():
    with open('build.cmd', 'w') as file:
        file.write('call "C:\\Program Files\\Microsoft Visual Studio 10.0\\VC\\vcvarsall.bat" x86\n')
        file.write('msbuild psiclient.sln /t:Rebuild /p:Configuration=Release')
    return subprocess.call('build.cmd')

if 0 == build_client():
    print 'Python: SUCCESS'
else:
    print 'Python: FAIL'
