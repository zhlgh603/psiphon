#!/usr/bin/python
#
# Copyright (c) 2016, Psiphon Inc.
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

import os
import datetime
import time
import shutil
import re
import json
import optparse

import sys
try:
    automation_dir = os.path.abspath('../')
    if os.path.exists(automation_dir):
        sys.path.insert(0, automation_dir)
    else:
        raise ("Could not insert Automation path.")
except:
    sys.exit()

import subprocess

import psi_ops

PSI_OPS_DB_FILENAME = os.path.join(os.path.abspath(automation_dir), 'psi_ops.dat')

import pynliner
from mako.template import Template
from mako.lookup import TemplateLookup
from mako import exceptions

MAKO_TEMPLATE = 'psi_mail_nagios_template.mako'
HOST_CHANGES_JSON_FILE = 'host_changes.json'
LOCAL_CONFIG_FILE = 'psi_ops_monitoring_config.json'

# Using the FeedbackDecryptor's mail capabilities
sys.path.append(os.path.abspath(os.path.join('..', '..', 'EmailResponder')))
sys.path.append(os.path.abspath(os.path.join('..', '..', 'EmailResponder', 'FeedbackDecryptor')))

import sender
from config import config


def load_local_config():
    '''
        Load a local config file.
    '''
    try:
        if os.path.exists(LOCAL_CONFIG_FILE):
            with open(LOCAL_CONFIG_FILE, 'r') as f:
                json_config_file_contents = json.loads(f.read())
        return json_config_file_contents
    except:
        print "Could not read in local config file: %s" % LOCAL_CONFIG_FILE
        sys.exit(1)


# Formats record using a mako template and sends email
def send_mail(record, subject='Nagios Host Monitoring', 
              template_filename=MAKO_TEMPLATE, emailRecipients=None):
    
    if not os.path.isfile(template_filename):
        raise
    
    template_lookup = TemplateLookup(directories=[os.path.dirname(os.path.abspath('__file__'))])
    template = Template(filename=template_filename, default_filters=['unicode', 'h'], lookup=template_lookup)
    
    try:
        rendered = template.render(data=record)
    except:
        raise Exception(exceptions.text_error_template().render())
    
    # CSS in email HTML must be inline
    rendered = pynliner.fromString(rendered)
    
    if emailRecipients is None:
        emailRecipients = config['emailRecipients']
    
    sender.send(emailRecipients, config['emailUsername'], subject, None, rendered)


def update_dat():
    '''
        Calls external script to update dat file.
    '''
    print "Updating psi_ops.dat"
    import psi_update_dat
    psi_update_dat.main(automation_dir)


def check_if_disabled(psinet, server):
    '''
        Checks if a server is disabled.  Currently not being used.
    '''
    propagation_channel = psinet.get_propagation_channel_by_id(server.propagation_channel_id)
    
    max_discovery_server_age_in_days = 0
    max_propagation_server_age_in_days = 0
    
    now = datetime.datetime.now()
    today = datetime.datetime(now.year, now.month, now.day)
    
    max_discovery_server_age_in_days = propagation_channel.max_discovery_server_age_in_days
    max_propagation_server_age_in_days = propagation_channel.max_propagation_server_age_in_days
    
    if max_discovery_server_age_in_days > 0:
        if (server.discovery_date_range and server.discovery_date_range[1] < (today - datetime.timedelta(days=max_discovery_server_age_in_days)) and psinet._PsiphonNetwork__hosts[server.host_id].provider in ['linode', 'digitalocean']):
            print 'Excluding server: %s' % (server.id)
            return True
    
    if max_propagation_server_age_in_days > 0:
        if not server.discovery_date_range and not server.is_embedded and server.logs[0][0] < (today - datetime.timedelta(days=max_propagation_server_age_in_days)) and psinet._PsiphonNetwork__hosts[server.host_id].provider in ['linode', 'digitalocean']:
            print 'Excluding server: %s' % (server.id)
            return True
    
    return False


def nagios_host_from_server(psinet, server, inheritFrom="psiphon3-server-template"):
    '''
        Creates a Nagios Host Object from Psiphon3 Server Entries
    '''
    hostGroups = ["psiphon3-servers"]
    
    host = psinet._PsiphonNetwork__hosts[server.host_id]
    
    # if not check_if_disabled(psinet, server):
    if host.provider in ['fasthosts'] and not server.is_permanent and not server.discovery_date_range:
        print 'Excluding server: %s on host: %s' % (server.id, server.host_id)
        return ''
    elif check_if_disabled(psinet, server):
        print 'Excluding disabled server: %s on host: %s' % (server.id, server.host_id)
        return ''
    else:
        hostGroups.append(psinet.get_propagation_channel_by_id(server.propagation_channel_id).name.lower().replace(" ", "_"))
        
        for c in server.capabilities:
            if server.capabilities[c]:
                hostGroups.append(c.lower())
        
        if server.is_embedded:
            hostGroups.append("embedded")
        
        if server.is_permanent:
            hostGroups.append("permanent")
        
        if server.discovery_date_range is not None:
            if server.discovery_date_range[1] > datetime.datetime.utcnow():
                hostGroups.append("discoverable")
        
        return '''define host {
        use               %s
        host_name         %s
        alias             %s
        hostgroups        %s
        address           %s
        _MGMT_PORT        %d
        _SSH_PORT         %d
        _OSSH_PORT        %d
        _MEEK_SERVER_PORT %d
        _WEB_SERVER_PORT  %d
        _STATS_USER       %s
        _STATS_PASS       %s
    }''' % (inheritFrom, 
            server.id, 
            server.id, 
            ",".join(hostGroups), 
            server.ip_address, 
            int(host.ssh_port or 0), 
            int(server.ssh_port or 0),
            int(server.ssh_obfuscated_port or 0),
            int(host.meek_server_port or 0),
            int(server.web_server_port or 0),
            host.stats_ssh_username, 
            host.stats_ssh_password)


def nagios_host_from_psiphon_host(host, inheritFrom="psiphon3-host-template"):
    '''
        Creates a Nagios Host Object from a Psiphon3 host entry
    '''
    hostGroups = ["psiphon3-hosts"]
        
    if host.provider:
        hostGroups.append(host.provider)
    
    if host.region:
        hostGroups.append(host.region)
    
    return '''define host {
    use                 %s
    host_name           %s
    alias               %s
    hostgroups          %s
    address             %s
    _MGMT_PORT          %d
}''' % (inheritFrom,
        host.id,
        host.id,
        ",".join(hostGroups),
        host.ip_address,
        int(host.ssh_port or 0),
        )


def nagios_service_from_psiphon_host(psinet, host):
    '''
        Creates a Nagios Service Object for each server on a host.
        
        Since there can be multiple servers on a host we need a way to check the 
        server services.  These custom service objects are specific to each host.
    '''
    servers = [s for s in psinet._PsiphonNetwork__servers.iteritems()
               if s[1].host_id == host.id]
    
    custom_services = list()
    
    for server in servers:
        for c in server[1].capabilities:
            if server[1].capabilities[c]:
                if c == 'handshake':
                    custom_services.append(define_custom_tcp_service(server[1].id, host.id, c, server[1].ip_address, server[1].web_server_port))
                if c == 'SSH':
                    custom_services.append(define_custom_tcp_service(server[1].id, host.id, c, server[1].ip_address, server[1].ssh_port))
                if c == 'OSSH':
                    custom_services.append(define_custom_tcp_service(server[1].id, host.id, c, server[1].ip_address, server[1].ssh_obfuscated_port))
                if c == 'VPN':
                    custom_services.append(define_custom_ipsec_service(server[1].id, host.id, c, server[1].ip_address, 500))
                    pass
                if c in ['FRONTED-MEEK', 'UNFRONTED-MEEK']:
                    custom_services.append(define_custom_tcp_service(server[1].id, host.id, c, server[1].ip_address, host.meek_server_port))
    
    return '\n'.join(custom_services)


def define_custom_tcp_service(server_id, host_id, service_name, server_ip, service_port):
    return '''define service {
    use                     %s
    service_description     %s
    host                    %s 
    check_command           %s
}''' % ('psiphon3-services', 
        '%s %s Service' % (server_id, service_name), 
        host_id, 
        'check_custom_tcp!%s!%s' % (server_ip, service_port))


def define_custom_ipsec_service(server_id, host_id, service_name, server_ip, service_port):
    return '''define service {
    use                     %s
    service_description     %s
    host                    %s 
    check_command           %s
}''' % ('psiphon3-services', 
        '%s %s Service' % (server_id, service_name), 
        host_id, 
        'check_custom_udp!%s!%s!-E -s "" -e ""' % (server_ip, service_port))


def nagios_hostgroup_from_psiphon_regions(psinet):
    hosts = psinet.get_hosts()
    regions = set()
    hostgroup_regions = set()
    for host in hosts:
        hostgroup_regions.add('''define hostgroup {
    hostgroup_name      %s
    alias               %s
}''' % (host.region, host.region))
        regions.add(host.region)
    
    hostgroup_regions.add('''define hostgroup {
    hostgroup_name      %s
    alias               %s
    hostgroup_members   %s
}''' % ('regions', 'Host Regions', ','.join(regions)))
    
    return '\n'.join(hostgroup_regions)


def nagios_hostgroup_from_psiphon_providers(psinet):
    hosts = psinet.get_hosts()
    providers = set()
    hostgroup_providers = set()
    for host in hosts:
        hostgroup_providers.add('''define hostgroup {
    hostgroup_name      %s
    alias               %s
}''' % (host.provider, host.provider))
        providers.add(host.provider)
    
    hostgroup_providers.add('''define hostgroup {
    hostgroup_name      %s
    alias               %s
    hostgroup_members   %s
}''' % ('providers', 'Host Providers', ','.join(providers)))
    
    return '\n'.join(hostgroup_providers)


def parse_nagios_host_entries(hosts):
    hosts_list = hosts.split('define host ')
    host_entries = list()
    for host in hosts_list:
        if host == '':
            continue
        entry = re.sub('({;)|(\n}\n)', '', 
                       re.sub(' +', ':', 
                              re.sub('\n( )+', ';', host)))
        host_entries.append(dict(i.split(':') for i in entry.split(';')))
    return host_entries


def generate_changed_psiphon_hosts_list(active_host_cfg, backup_host_cfg):
    with open(active_host_cfg, 'r') as f:
        current_hosts = f.read()

    f.close()
    active_host_entries = parse_nagios_host_entries(current_hosts)
    active_host_names = [h['host_name'] for h in active_host_entries]
        
    with open(backup_host_cfg, 'r') as f:
        backup_hosts = f.read()

    f.close()
    backup_host_entries = parse_nagios_host_entries(backup_hosts)
    backup_host_names = [h['host_name'] for h in backup_host_entries]
    
    added_hosts = list(set(active_host_names + backup_host_names) - set(backup_host_names))
    pruned_hosts = list(set(active_host_names + backup_host_names) - set(active_host_names))
    
    return(added_hosts, pruned_hosts)        


def save_host_changes(added_hosts, pruned_hosts):
    '''
        save_host_changes to a file to be sent as a status update daily.
    '''
    json_file = HOST_CHANGES_JSON_FILE
    if os.path.exists(json_file):
        with open(json_file, 'r') as f:
            json_data = json.loads(f.read())
    else:
        json_data = list()
    
    data = {
        'timestamp': time.time(), 
        'added_hosts': added_hosts,
        'pruned_hosts': pruned_hosts,
        }
        
    json_data.append(data)
    
    with open(json_file, 'w') as f:
        json.dump(json_data, f)


def email_host_changes(local_config):
    json_file = HOST_CHANGES_JSON_FILE
    with open(json_file, 'r') as f:
        data = json.loads(f.read())
    
    record = ('Psiphon 3 Nagios Daily Summary', data)
    
    send_mail(record, emailRecipients=local_config['email_recipients'])


def email_nagios_failure(local_config, err_lines):
    record = err_lines
    send_mail(
        record, 
        subject="Psiphon Nagios Service Failure", 
        template_filename='psi_mail_nagios_failure_template.mako',
        emailRecipients=local_config['email_recipients'])


def rotate_log_file(logfile=HOST_CHANGES_JSON_FILE):
    '''
        rotate_log_file will back up the (JSON) log file.
        By default this will be called daily
    '''
    if os.path.exists(logfile):
        shutil.move(logfile, (logfile + '.bak'))


def main(local_config):
    psinet = psi_ops.PsiphonNetwork.load_from_file(PSI_OPS_DB_FILENAME)
    active_cfg_path = os.path.join(os.path.abspath('.'), 'objects', 'psiphon', 'active')
    
    hosts = [nagios_host_from_psiphon_host(h) for h in psinet.get_hosts()]
    nagios_hosts = "\n".join(hosts)
    host_cfg_file = 'psiphon3-hosts.cfg'
    
    try:
        if not os.path.exists(active_cfg_path):
            raise OSError("Path not found")
        
        active_host_cfg = os.path.join(active_cfg_path, host_cfg_file)
        backup_host_cfg = active_host_cfg + '.bak'
        
        (added_hosts, pruned_hosts) = generate_changed_psiphon_hosts_list(active_host_cfg, backup_host_cfg)
        print "Added hosts: %s" % str(added_hosts)
        print "Pruned hosts: %s" % str(pruned_hosts)
        
        if os.path.isfile(active_host_cfg):
            shutil.copyfile(active_host_cfg, backup_host_cfg)
        
        with open(active_host_cfg, 'w') as file_out:
            file_out.write(nagios_hosts)
        
        custom_host_services = []
        for host in psinet.get_hosts():
            custom_host_services.append(nagios_service_from_psiphon_host(psinet, host))
        
        host_services = '\n'.join(custom_host_services)
        
        custom_host_services_cfg_file = 'host-services.cfg'
        active_custom_host_services_cfg = os.path.join(active_cfg_path, custom_host_services_cfg_file)
        backup_custom_host_services_cfg = active_custom_host_services_cfg + '.bak'
        if os.path.isfile(active_custom_host_services_cfg):
            shutil.copyfile(active_custom_host_services_cfg, backup_custom_host_services_cfg)
        
        with open(active_custom_host_services_cfg, 'w') as file_out:
            file_out.write(host_services)
        
        # hostgroup_regions config file
        hostgroup_regions_cfg_file = 'hostgroup_regions.cfg'
        hostgroup_regions = nagios_hostgroup_from_psiphon_regions(psinet)
        
        active_hostgroup_regions_cfg = os.path.join(active_cfg_path, hostgroup_regions_cfg_file)
        backup_hostgroup_regions_cfg = active_hostgroup_regions_cfg + '.bak'
        if os.path.isfile(active_hostgroup_regions_cfg):
            shutil.copyfile(active_hostgroup_regions_cfg, backup_hostgroup_regions_cfg)
        
        with open(active_hostgroup_regions_cfg, 'w') as file_out:
            file_out.write(hostgroup_regions)
        
        # hostgroup_providers config file
        hostgroup_providers_cfg_file = 'hostgroup_providers.cfg'
        hostgroup_providers = nagios_hostgroup_from_psiphon_providers(psinet)
        
        active_hostgroup_providers_cfg = os.path.join(active_cfg_path, hostgroup_providers_cfg_file)
        backup_hostgroup_providers_cfg = active_hostgroup_providers_cfg + '.bak'
        if os.path.isfile(active_hostgroup_providers_cfg):
            shutil.copyfile(active_hostgroup_providers_cfg, backup_hostgroup_providers_cfg)
        
        with open(active_hostgroup_providers_cfg, 'w') as file_out:
            file_out.write(hostgroup_providers)
        
        nagios_reload_cmd = ['sudo', 'service', 'nagios', 'reload']
        subprocess.check_output(nagios_reload_cmd)
        
        if len(added_hosts) > 0 or len(pruned_hosts) > 0:
            save_host_changes(added_hosts, pruned_hosts)
        
    except IOError, e:
        print str(e)
    except OSError, e:
        print str(e)
    except subprocess.CalledProcessError, e:
        email_nagios_failure(local_config, e.output)
    except Exception, e:
        print str(e)


if __name__ == "__main__":
    parser = optparse.OptionParser('usage: %prog [options]')
    parser.add_option("-m", "--send_mail", action="store_true", help="Send mail summary")
    parser.add_option("-r", "--rotate_logs", action="store_true", help="Rotate log file")
    parser.add_option("-u", "--update_hosts", action="store_true", help="Update hosts for monitoring")
    
    (options, _) = parser.parse_args()
    
    local_config = load_local_config()
    
    if options.send_mail:
        email_host_changes(local_config)
    if options.rotate_logs:
        rotate_log_file()
    if options.update_hosts:
        update_dat()
        main(local_config)
