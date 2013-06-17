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
from psi_ssh_connection import SSHConnection, OSSHConnection
import json
import os

import optparse


# TODO: add support to server for indicating platform
CLIENT_VERSION = 1
LOCAL_SOCKS_PORT = 1080
LOCAL_HOST_IP = '127.0.0.1'
GLOBAL_HOST_IP = '0.0.0.0'



class Data(object):

    def __init__(self, data):
        self.data = data

    @staticmethod
    def load():
        try:
            with open('psi_client.dat', 'r') as data_file:
                data = Data(json.loads(data_file.read()))
            # Validate
            data.servers()[0]
            data.propagation_channel_id()
            data.sponsor_id()
        except (IOError, ValueError, KeyError, IndexError, TypeError) as error:
            print '\nPlease obtain a valid psi_client.dat file and try again.\n'
            raise
        return data

    def save(self):
        with open('psi_client.dat', 'w') as data_file:
            data_file.write(json.dumps(self.data))

    def servers(self):
        return self.data['servers']

    def propagation_channel_id(self):
        return self.data['propagation_channel_id']

    def sponsor_id(self):
        return self.data['sponsor_id']

    def move_first_server_entry_to_bottom(self):
        servers = self.servers()
        if len(servers) > 1:
            servers.append(servers.pop(0))
            return True
        else:
            return False


def connect_to_server(data, relay, tun_type='local'):

    assert relay in ['SSH', 'OSSH']

    server = Psiphon3Server(data.servers(), data.propagation_channel_id(), data.sponsor_id(), CLIENT_VERSION)
    handshake_response = server.handshake(relay)
    # handshake might update the server list with newly discovered servers
    data.save()

    home_pages = handshake_response['Homepage']
    if len(home_pages) > 0:
        print '\nPlease visit our sponsor\'s homepage%s:' % ('s' if len(home_pages) > 1 else '',)
    for home_page in home_pages:
        print home_page

    if tun_type == 'global':
        listen_address=GLOBAL_HOST_IP
    else:
    
        listen_address=LOCAL_HOST_IP

    if relay == 'OSSH':
        ssh_connection = OSSHConnection(server.ip_address, handshake_response['SSHObfuscatedPort'],
                                        handshake_response['SSHUsername'], handshake_response['SSHPassword'],
                                        handshake_response['SSHObfuscatedKey'], handshake_response['SSHHostKey'],
                                        str(LOCAL_SOCKS_PORT), str(listen_address))
    else:
        ssh_connection = SSHConnection(server.ip_address, handshake_response['SSHPort'],
                                       handshake_response['SSHUsername'], handshake_response['SSHPassword'],
                                       handshake_response['SSHHostKey'], str(LOCAL_SOCKS_PORT), str(listen_address))
    ssh_connection.connect()
    ssh_connection.test_connection()
    server.connected(relay)
    ssh_connection.wait_for_disconnect()
    server.disconnected(relay)


def connect(tunnel_type):

    while True:

        data = Data.load()

        try:
            if os.path.isfile('./ssh'):
                connect_to_server(data, 'OSSH', tunnel_type)
            else:
                connect_to_server(data, 'SSH', tunnel_type)
            break
        except Exception as error:
            print error
            if not data.move_first_server_entry_to_bottom():
                break
            data.save()
            print 'Trying next server...'


if __name__ == "__main__":
    parser = optparse.OptionParser('usage: %prog [options]')
    parser.add_option("--local", "-l", dest="local_tunnel", 
                        action="store_true", help="tunnel local connections")
    parser.add_option("--global", "-g", dest="global_tunnel",     
                        action="store_true", help="global tunnel")
    (options, _) = parser.parse_args()
    
    if options.local_tunnel:
        connect('local')
    elif options.global_tunnel:
        connect('global')
    else:
        connect('local')
        
    # connect()

