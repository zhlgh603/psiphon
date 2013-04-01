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


import pexpect
import hashlib
import base64
import sys


class SSHConnection(object):

    def __init__(self, ip_address, port, username, password, host_key, listen_port):
        self.ip_address = ip_address
        self.port = port
        self.username = username
        self.password = password
        self.host_key = host_key
        self.listen_port = listen_port
        self.ssh = None

    def __del__(self):
        if self.ssh:
            self.ssh.terminate()

    # Get the RSA key fingerprint from the host's SSH_Host_Key
    # Based on:
    # http://stackoverflow.com/questions/6682815/deriving-an-ssh-fingerprint-from-a-public-key-in-python
    def _ssh_fingerprint(self):
        base64_key = base64.b64decode(self.host_key)
        md5_hash = hashlib.md5(base64_key).hexdigest()
        return ':'.join(a + b for a, b in zip(md5_hash[::2], md5_hash[1::2]))

    def connect(self):
        self.ssh = pexpect.spawn('ssh -D %s -N -p %s %s@%s' %
                                 (self.listen_port, self.port, self.username, self.ip_address))
        # Print ssh output:
        #self.ssh.logfile_read = sys.stdout
        prompt = self.ssh.expect([self._ssh_fingerprint(), 'Password:'])
        if prompt == 0:
            self.ssh.sendline('yes')
            self.ssh.expect('Password:')
            self.ssh.sendline(self.password)
        else:
            self.ssh.sendline(self.password)

    def test_connection(self):
        # TODO: test the connection
        print '\nYour SOCKS proxy is now running at 127.0.0.1:%s' % (self.listen_port,)

    def wait_for_disconnect(self):
        try:
            print 'Press Ctrl-C to terminate.'
            self.ssh.wait()
        except KeyboardInterrupt as e:
            print 'Terminating...'
            self.ssh.terminate()

        print 'Connection closed'


class OSSHConnection(SSHConnection):

    def __init__(self, ip_address, port, username, password, obfuscate_keyword, host_key, listen_port):
        self.obfuscate_keyword = obfuscate_keyword
        SSHConnection.__init__(self, ip_address, port, username, password, host_key, listen_port)

    def connect(self):
        self.ssh = pexpect.spawn('./ssh -D %s -N -p %s -z -Z %s %s@%s' %
                                 (self.listen_port, self.port, self.obfuscate_keyword,
                                  self.username, self.ip_address))
        # Print ssh output:
        #self.ssh.logfile_read = sys.stdout
        prompt = self.ssh.expect([self._ssh_fingerprint(), 'Password:'])
        if prompt == 0:
            self.ssh.sendline('yes')
            self.ssh.expect('Password:')
            self.ssh.sendline(self.password)
        else:
            self.ssh.sendline(self.password)


