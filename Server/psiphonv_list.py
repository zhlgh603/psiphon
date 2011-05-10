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

'''

Example output (using test data spreadsheet):

> list.py embed
31302e302e302e3230203830204638393446333333393035463833313720<encoded server cert>
3139322e3136382e302e3230203830203838454632464434333234373235354220<encoded server cert>
3137322e31362e302e3230203830203346453334423839313242383835424220<encoded server cert>

> list.py handshake 127.0.0.1 3A885577DD84EF13 0
Homepage: http://news.google.com/news?pz=0&hl=en&ned=ca
Upgrade: 1
Server: 31302e302e302e3230203830204638393446333333393035463833313720<encoded server cert>
Server: 3139322e3136382e302e3230203830203838454632464434333234373235354220<encoded server cert>
Server: 3137322e31362e302e3230203830203346453334423839313242383835424220<encoded server cert>

'''

import xlrd
import binascii
import sys


FILENAME = 'master_list.xls'

SERVER_LIST_SHEET_NAME = u'Server List'
SERVER_LIST_SHEET_COLUMNS = [
    u'IP Address',
    u'Web Server Port',
    u'Server Secret',
    u'Server Certificate',
    u'SSH username',
    u'SSH password',
    u'Notes'
]

SPONSOR_LIST_SHEET_NAME = u'Sponsor List'
SPONSOR_LIST_SHEET_COLUMNS = [
    u'Client ID',
    u'Region',
    u'Home Page URL',
    u'Notes'
]

UPGRADE_LIST_SHEET_NAME = u'Upgrade List'
UPGRADE_LIST_SHEET_COLUMNS = [
    u'Client ID',
    u'Client Version',
    u'Notes'
]


def get_encoded_server_list():
    xls = xlrd.open_workbook(FILENAME)
    server_list = xls.sheet_by_name(SERVER_LIST_SHEET_NAME)
    assert([cell.value for cell in server_list.row(0)] == SERVER_LIST_SHEET_COLUMNS)
    encoded_server_list = []
    for i in range(1, server_list.nrows):
        server = server_list.row(i)
        ip_address = server[0].value.encode('utf-8')
        web_server_port = server[1].value.encode('utf-8')
        server_secret = server[2].value.encode('utf-8')
        server_certificate = server[3].value.encode('utf-8')
        server_entry = '%s %s %s %s' % (ip_address, web_server_port, server_secret, server_certificate)
        hex_server_entry = binascii.hexlify(server_entry)
        encoded_server_list.append(hex_server_entry)
    return encoded_server_list


def get_homepage(client_id, region):
    xls = xlrd.open_workbook(FILENAME)
    sponsor_list = xls.sheet_by_name(SPONSOR_LIST_SHEET_NAME)
    assert([cell.value for cell in sponsor_list.row(0)] == SPONSOR_LIST_SHEET_COLUMNS)
    for i in range(1, sponsor_list.nrows):
        sponsor = sponsor_list.row(i)
        sponsor_client_id = sponsor[0].value.encode('utf-8')
        sponsor_region = sponsor[1].value.encode('utf-8')
        if sponsor_client_id == client_id and sponsor_region == region:
            return sponsor[2].value.encode('utf-8')
    return None


def get_upgrade(client_version, client_id):
    xls = xlrd.open_workbook(FILENAME)
    upgrade_list = xls.sheet_by_name(UPGRADE_LIST_SHEET_NAME)
    assert([cell.value for cell in upgrade_list.row(0)] == UPGRADE_LIST_SHEET_COLUMNS)
    for i in range(1, upgrade_list.nrows):
        upgrade = upgrade_list.row(i)
        upgrade_client_id = upgrade[0].value.encode('utf-8')
        upgrade_client_version = upgrade[1].value.encode('utf-8')
        if upgrade_client_id == client_id:
            if int(client_version) < int(upgrade_client_version):
                return upgrade_client_version
            return None
    return None


def handshake(client_ip_address, client_id, client_version):
    # TODO: geolocate client_ip_address
    # TODO: real discovery, etc.
    output = []
    region = 'CA'
    homepage_url = get_homepage(client_id, region)
    if homepage_url:
        output.append('Homepage: %s' % (homepage_url,))
    upgrade_client_version = get_upgrade(client_version, client_id)
    if upgrade_client_version:
        output.append('Upgrade: %s' % (upgrade_client_version,))
    for encoded_server_entry in get_encoded_server_list():
        output.append('Server: %s' % (encoded_server_entry,))
    return output


def embed():
    return get_encoded_server_list()


if __name__ == "__main__":
    # TODO: graceful error handling
    args = sys.argv[1:]
    if args[0] == 'embed':
        for line in embed(): print line
    elif args[0] == 'handshake':
        client_ip_address = args[1]
        client_id = args[2]
        client_version = args[3]
        for line in handshake(client_ip_address, client_id, client_version): print line
    else:
        assert(0)
