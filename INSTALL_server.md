# Psiphon3 Server Installation

**Pre-requisites: Follow INSTALL_host.md**

1. Create a Psiphon Host object
2. Create a Psiphon Server object.  Not all values are mandatory.

## Psiphon3 Host Object

* Most values can be gleaned from INSTALL_host.md document
* Mandatory Host field types:
  ```
host_id = str()                 # Unique Identifier
provider = str()                # Host VPS Provider
provider_id = str()             # Unique Provider host name
ip_address = str()              # IP address of server
ssh_port = int()                # Management ssh port
ssh_username = str()            # Admin user name
ssh_password = str()            # Admin password
ssh_host_key = str()            # Host key, usually contents of ssh_host_rsa_key.pub
stats_ssh_username = str()      # Stats user name
stats_ssh_password = str()      # Stats user password
datacenter_name = str()         # Datacenter identifier
region = str()                  # Country region of datacenter (US,GB,etc)
  ```

* If *INSTALL_host.md* is not followed, examine `provider_launch_new_server()`. 
  It will return most of the host information necessary.
 * data returned can be put into a host object 
   ```
data = provider_launch_new_server()
host = psinet.get_host_object(*data)
   ```

* Mandatory Server field types (some can be passed `NoneType`)
  ```
s_id = str()
s_host_id = str()                           # `host.id`
s_ip_address = str()                        # Typically `host.ip_address`
s_egress_ip_address = str()                 # Typically `host.ip_address`
s_internal_ip_address = str()               # Typically `host.ip_address`
s_propagation_channel_id = str()            # Propagation channel ID
s_is_embedded = bool()                      # If embedded in client build
s_is_permanent = bool()                     # If server is permanent
s_discovery_date_range = datetime           # Range when the server can be discovered
s_capabilities = dict(type: bool)           # Dict with transport type and if enabled 
s_web_server_port = int()                   # Web server port for handshake
s_web_server_secret = str()                 # Web server secret for handshake
s_web_server_certificate = str()            # Certificate used in handshake
s_web_server_private_key = str()            # Web server private key
s_ssh_port = int()                          # SSH capability port
s_ssh_username = str()                      # SSH capability user name
s_ssh_password = str()                      # SSH capability password
s_ssh_host_key = str()                      # SSH capability host key
s_ssh_obfuscated_port = str()               # Obfuscated SSH capability port
s_ssh_obfuscated_key = str()                # Obfuscated SSH capability key
s_alternate_ssh_obfuscated_ports = list()   # Alternate Obfuscated SSH ports
  ```

* Once host and server objects are defined:
  ```
record = psinet.setup_server(host, [server])
  ```

