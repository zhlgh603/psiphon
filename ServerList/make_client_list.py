#!/bin/python
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

import xlrd
import binascii

FILENAME = 'master_list.xls'

SERVER_LIST_SHEET_NAME = u'Server List'
SERVER_LIST_SHEET_COLUMNS = [
    u'IP Address',
    u'Web Server Port',
    u'Server Secret',
    u'SSH username',
    u'SSH password',
    u'Notes'
]

xls = xlrd.open_workbook(FILENAME)

server_list = xls.sheet_by_name(SERVER_LIST_SHEET_NAME)

assert([cell.value for cell in server_list.row(0)] == SERVER_LIST_SHEET_COLUMNS)

# Output client-encoded server values

for i in range(1, server_list.nrows):
    server = server_list.row(i)
    ip_address = server[0].value.encode('utf-8')
    web_server_port = server[1].value.encode('utf-8')
    server_secret = server[2].value.encode('utf-8')
    server_entry = '%s %s %s' % (ip_address, web_server_port, server_secret)
    hex_server_entry = binascii.hexlify(server_entry)
    print hex_server_entry

# (TODO: discovery, etc.)

