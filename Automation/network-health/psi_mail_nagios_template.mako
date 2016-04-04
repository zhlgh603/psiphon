## Copyright (c) 2014, Psiphon Inc.
## All rights reserved.
##
## This program is free software: you can redistribute it and/or modify
## it under the terms of the GNU General Public License as published by
## the Free Software Foundation, either version 3 of the License, or
## (at your option) any later version.
##
## This program is distributed in the hope that it will be useful,
## but WITHOUT ANY WARRANTY; without even the implied warranty of
## MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
## GNU General Public License for more details.
##
## You should have received a copy of the GNU General Public License
## along with this program.  If not, see <http://www.gnu.org/licenses/>.
<style>
    table {
        padding: 0;
        border: 1px solid black;
        width: 100%
    }
    
    table tr {
        border: 0;
        border-top: 1px solid #CCC;
        background-color: white;
        margin: 0;
        /*padding: 0; */
    }
    
    table, tbody tr, td{
        border: 1px solid black;
        padding-left: 5px;
        vertical-align: top;
        padding-top: 5px;
    }
</style>

<h1>Psiphon 3 Nagios Monitoring</h1>
<%

added_hosts, pruned_hosts, nagios_failed, runtime = data

added_hosts_count = len(added_hosts)
pruned_hosts_count = len(pruned_hosts)

%>

<h3>Runtime: ${runtime}</h3>
% if nagios_failed:
    <h3>Nagios Failure</h3>
% endif

<h3>Added Hosts: ${added_hosts_count}</h3>
% if added_hosts_count > 0:
    <table>
    <thead>
        <tr>
            <th>Host name</th>
            <th>Provider</th>
        </tr>
    </thead>
    <tbody>
        % for host_id in added_hosts:
            <tr>
                <td>${host_id}</td>
                % if host_id.startswith('do-''):
                    <td>digitalocean</td>
                % elif host_id.startswith('li'):
                    <td>linode</td>
                % elif host_id.startswith('vn-'):
                    <td>vps.net</td>
                % elif host_id.startswith('fh-'):
                    <td>fasthosts</td>
                % else:
                    <td>unknown</td>
                % endif
            </tr>
        % endfor
    </tbody>
    </table>
% endif

<h3>Pruned Hosts: ${pruned_hosts_count}</h3>
% if pruned_hosts_count > 0:
    <table>
    <thead>
        <tr>
            <th>Host name</th>
            <th>Provider</th>
        </tr>
    </thead>
    <tbody>
        % for host_id in pruned_hosts:
            <tr>
                <td>${host_id}</td>
                % if 'do-' in host_id:
                    <td>digitalocean</td>
                % elif 'li-' in host_id:
                    <td>linode</td>
                % elif 'vn-' in host_id:
                    <td>vps.net</td>
                % elif 'fh-' in host_id:
                    <td>fasthosts</td>
                % else:
                    <td>unknown</td>
                % endif
            </tr>
        % endfor
    </tbody>
    </table>
% endif

