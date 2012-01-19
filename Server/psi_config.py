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

#==== Directory Layout =========================================================

HOST_SOURCE_ROOT = '/opt/PsiphonV'
HOST_IP_UP_DIR = '/etc/ppp/ip-up.d'
HOST_IP_DOWN_DIR = '/etc/ppp/ip-down.d'
HOST_INIT_DIR = '/etc/init.d'


#==== Web Server ==============================================================

import os
import posixpath

UPGRADE_DOWNLOAD_PATH = '/opt/PsiphonV/download'
ROUTES_PATH = '/opt/PsiphonV/routes'
ROUTE_FILE_NAME_TEMPLATE = '%s.route.zlib'
DATA_FILE_NAME = posixpath.join(HOST_SOURCE_ROOT, 'Automation', 'psi_ops.dat')


#==== VPN =====================================================================

IPSEC_PSK_LENGTH = 32
IPSEC_SECRETS_FILENAME = '/etc/ipsec.secrets'


#==== Syslog ==================================================================

try:
    import syslog

    SYSLOG_IDENT = 'psiphonv'
    SYSLOG_FACILITY = syslog.LOG_USER

except ImportError:
    pass
