# README

The *network-health* package is used to generate config files for a Nagios 4
monitoring system to use.  This file docuements how this process works.

Basic set up and notes:
* Clone this as a sub directory of *psiphon-circumvention-system/Automation*.
* Modify *nagios.cfg* to use the custom config paths.
  ```
# Config for Psiphon Hosts
cfg_dir=/opt/Psiphon/psiphon-circumvention-system/Automation/network-health/objects/psiphon/ec2
cfg_dir=/opt/Psiphon/psiphon-circumvention-system/Automation/network-health/objects/psiphon/active
cfg_dir=/opt/Psiphon/psiphon-circumvention-system/Automation/network-health/objects/psiphon/templates

# Psiphon Command config file
cfg_dir=/opt/Psiphon/psiphon-circumvention-system/Automation/network-health/objects/psiphon/commands
cfg_dir=/opt/Psiphon/psiphon-circumvention-system/Automation/network-health/objects/psiphon/contacts
# Definitions for monitoring a Windows machine
  ```

* The *psi_ops.dat* synchronization task is handled by running `psi_update_dat.py`
  which is not included in this package.
* The sub-folder tree is an attempt to organize the config files.

Folder structure:

```
- monitoring
|- objects
 |- psiphon
  |- active
  |- commands
  |- contacts
  |- ec2
  |- inactive
  |- samples
  |- templates
 |- scripts
```

## Objects

### ACTIVE

The `active` sub-dir contains active files that are modified often.  They are:
* A host config file, that is changed as Psiphon hosts are added or removed. i.e. `psiphon3-hosts.cfg`.
* A services config file that is overwritten on evey host change. i.e. `host-services.cfg`.
* Hostgroup config files that groups hosts. i.e. `hostgroup_regions.cfg`, `hostgroup_providers.cfg`


#### `psiphon3-hosts.cfg`

Contains Psiphon hosts that are to be monitored.  `_MGMT_PORT` is the system
management port which is used to check if the system is up and reachable.
The `_MGMT_PORT` is used because we do not allow ICMP ECHO.

* SAMPLE

  ```
define host {
    use                 psiphon3-host-template      # host template
    host_name           system-hostname
    alias               system-alias
    hostgroups          # hostgroup_region, hostgroup_provider
    address             XXX.XXX.XXX.XXX
    _MGMT_PORT          XXXX
}
  ```

#### `host-services.cfg`

Contains Psiphon server service details.  There is a unique service entry for 
each Psiphon server capability.  I.E. a Psiphon server that offers OSSH and 
SSH would have 2 service entries.

The `check_command` is a simple TCP check that is passed the server IP and 
port to check. 


* SAMPLE

  ```
define service {
    use                     psiphon3-services
    service_description     # server name and service
    host                    system-hostname
    check_command           check_custom_tcp!<ipaddr>!<tcpport>
}
  ```

### COMMANDS

The commands sub-dir contains a file `commands.cfg` that define commands used 
by Psiphon service and host definitions to check the health of a host or server.

* SAMPLE

  ```
define command {
    command_name    check-mgmt-port
    command_line    $USER1$/check_ssh -H $HOSTADDRESS$ -p $_HOSTMGMT_PORT$
}
  ```

### CONTACTS
### EC2
### CONTACTS
### SAMPLES
### TEMPLATES

## Scripts

This folder contains customized scripts that are on hosts for monitoring services on 
hosts.
