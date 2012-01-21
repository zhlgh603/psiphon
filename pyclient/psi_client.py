#!/usr/bin/python
#
# Copyright (c) 2012, Psiphon Inc.
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


from psi_api import Psiphon3Server
from psi_ssh_connection import SSHConnection
import json
import binascii


def connect_to_server(ip_address, web_server_port, web_server_secret,
                      web_server_certificate, propagation_channel_id, sponsor_id):

    server = Psiphon3Server(ip_address, web_server_port, web_server_secret,
                            web_server_certificate, propagation_channel_id, sponsor_id)
    handshake_response = server.handshake()

    home_pages = handshake_response['Homepage']
    if len(home_pages) > 0:
        print '\nPlease visit our sponsor\'s homepage%s:' % ('s' if len(home_pages) > 1 else '',)
    for home_page in home_pages:
        print home_page

    ssh_connection = SSHConnection(server.ip_address, handshake_response['SSHPort'],
                                   handshake_response['SSHUsername'], handshake_response['SSHPassword'],
                                   handshake_response['SSHHostKey'], '1080')
    ssh_connection.connect()


def load_data():

    with open('psi_client.dat', 'r') as data_file:
        data = json.loads(data_file.read())
        return data


def save_data(data):

    with open('psi_client.dat', 'w') as data_file:
        data_file.write(json.dumps(data))


def move_first_server_entry_to_bottom():

    data = load_data()
    servers = data['servers']
    if len(servers) > 1:
        servers.append(servers.pop(0))
        save_data(data)
        return True
    else:
        return False


def connect():

    while True:
        try:
            data = load_data()
            propagation_channel_id = data['propagation_channel_id']
            sponsor_id = data['sponsor_id']
            server_entry = binascii.unhexlify(data['servers'][0]).split(" ")
        except (IOError, ValueError, KeyError, TypeError) as error:
            print '\nPlease obtain a valid psi_client.dat file and try again.\n'
            raise

        try:
            connect_to_server(*server_entry, propagation_channel_id=propagation_channel_id, sponsor_id=sponsor_id)
            break
        except Exception as error:
            print error
            if not move_first_server_entry_to_bottom():
                break
            print 'Trying next server...'


if __name__ == "__main__":

    connect()


