# Ubuntu Server Host Installation

## Dependencies

* Ubuntu 14.04(.3)
* Python 2.7.X and modules

## Add stats user for log collection

* Generate a stats user
```
STATS_USER="stats-"$(< /dev/urandom tr -dc _A-Z-a-z-0-9 | head -c${1:-10};echo;)
STATS_PASS=$(< /dev/urandom tr -dc _A-Z-a-z-0-9 | head -c${1:-24};echo;)

echo $STATS_USER - $STATS_PASS

useradd -g adm -d /var/log $STATS_USER
echo "$STATS_USER:$STATS_PASS" | chpasswd
```

### 1. Configure SSH, Firewall, IP Forwarding

Update SSH configuration to bind to only one IP address and use port 2222
(or other administrative port).  If not port 2222 the below firewall rules
will need to be changed.

#### Configure SSH

* Modify `/etc/ssh/sshd_config`
 * Change SSH listener port: 
    ```
    sed -i 's/Port 22/Port 2222/' /etc/ssh/sshd_config
    ```

 * Set ListenAddress for management interface only: 
    ```
    ListenAddress X.X.X.X`
    ```
   
 * Disable SSH checking ECDSA key: 
    ```
    sed -i 's/^HostKey \/etc\/ssh\/ssh_host_ecdsa_key/#&/' /etc/ssh/sshd_config
    sed -i 's/^HostKey \/etc\/ssh\/ssh_host_ed25519_key/#&/' /etc/ssh/sshd_config
    ```
    
 * **Optional** Set AllowUsers:
    ```
    echo "AllowUsers root $STATS_USER" >> /etc/ssh/sshd_config
    ```

 * **Optional** regenerate ssh key:
    ```
    ssh-keygen -t rsa -f /etc/ssh/ssh_host_rsa_key
    ```


#### Set up iptables
* **NOTE** These rules assume `ethX` adjust for `venetX` if necessary

* Add our default iptables.rules:
    ```
    cat << EOF > /etc/iptables.rules
    *filter
      -A INPUT -i lo -j ACCEPT
      -A INPUT -d 127.0.0.0/8 ! -i lo -j REJECT --reject-with icmp-port-unreachable
      -A INPUT -m state --state RELATED,ESTABLISHED -j ACCEPT
      -A INPUT -p tcp -m state --state NEW -m tcp --dport 22 -j ACCEPT
      -A INPUT -p tcp -m state --state NEW -m tcp --dport 80 -j ACCEPT
      -A INPUT -p tcp -m state --state NEW -m tcp --dport 465 -j ACCEPT
      -A INPUT -p tcp -m state --state NEW -m tcp --dport 587 -j ACCEPT
      -A INPUT -p tcp -m state --state NEW -m tcp --dport 993 -j ACCEPT
      -A INPUT -p tcp -m state --state NEW -m tcp --dport 995 -j ACCEPT
      -A INPUT -p tcp -m state --state NEW -m tcp --dport 2222 -j ACCEPT
      -A INPUT -p tcp -m state --state NEW -m tcp --dport 8080 -j ACCEPT
      -A INPUT -p esp -j ACCEPT
      -A INPUT -p ah -j ACCEPT
      -A INPUT -p udp --dport 500 -j ACCEPT
      -A INPUT -p udp --dport 4500 -j ACCEPT
      -A INPUT -i ipsec+ -p udp -m udp --dport l2tp -j ACCEPT
      -A INPUT -j REJECT --reject-with icmp-port-unreachable
      -A FORWARD -s 10.0.0.0/8 -p tcp -m multiport --dports 80,443,554,1935,7070,8000,8001,6971:6999 -j ACCEPT
      -A FORWARD -s 10.0.0.0/8 -p udp -m multiport --dports 80,443,554,1935,7070,8000,8001,6971:6999 -j ACCEPT
      -A FORWARD -s 10.0.0.0/8 -d 8.8.8.8 -p tcp --dport 53 -j ACCEPT
      -A FORWARD -s 10.0.0.0/8 -d 8.8.8.8 -p udp --dport 53 -j ACCEPT
      -A FORWARD -s 10.0.0.0/8 -d 8.8.4.4 -p tcp --dport 53 -j ACCEPT
      -A FORWARD -s 10.0.0.0/8 -d 8.8.4.4 -p udp --dport 53 -j ACCEPT
      -A FORWARD -s 10.0.0.0/8 -d 10.0.0.0/8 -j DROP
      -A FORWARD -s 10.0.0.0/8 -j DROP 
      -A OUTPUT -p tcp -m multiport --dports 53,80,443,554,1935,7070,8000,8001,6971:6999 -j ACCEPT
      -A OUTPUT -p udp -m multiport --dports 53,80,443,554,1935,7070,8000,8001,6971:6999 -j ACCEPT
      -A OUTPUT -p udp -m udp --dport 123 -j ACCEPT
      -A OUTPUT -p tcp -m tcp --sport 22 -j ACCEPT
      -A OUTPUT -p tcp -m tcp --sport 80 -j ACCEPT
      -A OUTPUT -p tcp -m tcp --sport 465 -j ACCEPT
      -A OUTPUT -p tcp -m tcp --sport 587 -j ACCEPT
      -A OUTPUT -p tcp -m tcp --sport 993 -j ACCEPT
      -A OUTPUT -p tcp -m tcp --sport 995 -j ACCEPT
      -A OUTPUT -p tcp -m tcp --sport 2222 -j ACCEPT
      -A OUTPUT -p tcp -m tcp --sport 8080 -j ACCEPT
      -A OUTPUT -p esp -j ACCEPT
      -A OUTPUT -p ah -j ACCEPT
      -A OUTPUT -p udp --sport 500 -j ACCEPT
      -A OUTPUT -p udp --sport 4500 -j ACCEPT
      -A OUTPUT -o ipsec+ -p udp -m udp --dport l2tp -j ACCEPT
      -A OUTPUT -j DROP
    COMMIT
    
    *nat
      -A PREROUTING -i eth+ -p tcp --dport 443 -j DNAT --to-destination :8080
      -A POSTROUTING -s 10.0.0.0/8 -o eth+ -j MASQUERADE
    COMMIT
    
    EOF
    ```
 
* Create a firewall script
    ```
     cat << EOF > /etc/network/if-up.d/firewall
    #!/bin/sh
    
    iptables-restore < /etc/iptables.rules
    EOF
    ```

 * Make it executable: `chmod +x /etc/network/if-up.d/firewall`

 * **Provider default firewall changes** 
    * Default firewall rules are set by: `/etc/sysconfig/firewall`.  Allow mangement port, or remove the default rules: `rm /etc/sysconfig/firewall`


#### IP Forwarding
* Allow IP Forwarding:
    ```
    sed -i 's/^#net.ipv4.ip_forward=1/net.ipv4.ip_forward=1/' /etc/sysctl.conf`
    ```


#### Restart modified services
```
/etc/network/if-up.d/firewall
service procps restart
service ssh restart
```

### 2. Install xinetd, wondershaper, fail2ban, ntp

```
apt-get update
apt-get dist-upgrade
apt-get install -y -f xinetd wondershaper fail2ban ntp
```

As management SSH access is open to any source IP address, we recommend
installing fail2ban to defend against brute-force SSH login attempts.

* Create or edit `jail.local` file.

    ```
    cat << EOF > /etc/fail2ban/jail.local
    [ssh]
    port    = ssh,80,465,587,993,995,2222
    
    [ssh-ddos]
    port    = ssh,80,465,587,993,995,2222
    
    EOF
    ```

* Edit `/etc/fail2ban/filter.d/sshd.conf`.  Add following line to failregex list:
   ```
        ^%(__prefix_line)spam_unix\(sshd:auth\): authentication failure; logname=\S* uid=\S* euid=\S* tty=\S* ruser=\S* rhost=<HOST>(?:\s+user=.*)?\s*$
   ```

* Set firewall script to restart fail2ban: `echo "/etc/init.d/fail2ban restart" >> /etc/network/if-up.d/firewall`

### 3. Install Obfuscated SSH Package
```
apt-get install libpam0g-dev build-essential unzip libssl-dev -f -y
wget https://github.com/brl/obfuscated-openssh/zipball/master -O ossh.zip
unzip ossh.zip
cd brl-obfuscated-openssh-ca93a2c
./configure --with-pam
make
make install
cd ..
```

### 4. Install and configure sudo
```
apt-get update
apt-get install sudo
```

Add the following line under "User privilege specification" below the root entry:
`www-data ALL=NOPASSWD: /usr/local/sbin/ipsec auto --rereadsecrets`
Confirm that the path to ipsec is correct. (`which ipsec`)

OR as a one liner: 
```
sed -i '/root.*ALL$/a \www-data ALL=NOPASSWD: \/usr\/local\/sbin\/ipsec auto --rereadsecrets' /etc/sudoers
```

### 5. Install xl2tp and python packages
```
apt-get update
apt-get install -y xl2tpd
apt-get install -y python-setuptools python-dev build-essential

easy_install cherrypy webob xlrd portalocker netifaces jsonpickle

```

#### Patch cherrypy
* Check what version of CherryPy is installed and patch `ssl_builtin.py`:
    ```
    vi /usr/local/lib/python2.7/dist-packages/CherryPy-3.8.0-py2.7.egg/cherrypy/wsgiserver/ssl_builtin.py`
    ```

    ```
    def wrap(self, sock):
        """Wrap and return the given socket, plus WSGI environ entries."""
        try:
            s = ssl.wrap_socket(sock, do_handshake_on_connect=True,
                    server_side=True, certfile=self.certificate,
                    keyfile=self.private_key, ssl_version=ssl.PROTOCOL_SSLv23)
        except ssl.SSLError, e:
            if e.errno == ssl.SSL_ERROR_EOF:
                # This is almost certainly due to the cherrypy engine
                # 'pinging' the socket to assert it's connectable;
                # the 'ping' isn't SSL.
                return None, {}
            elif e.errno == ssl.SSL_ERROR_SSL:
                if e.args[1].endswith('http request'):
                    # The client is speaking HTTP to an HTTPS server.
                    raise wsgiserver.NoSSLError
            raise
    +        environ = self.get_environ(s)
    +        if environ:
    +            return s, environ
    +        else:
    +            return None, {}
    -        return s, self.get_environ(s)

    # TODO: fill this out more with mod ssl env
    def get_environ(self, sock):
        """Create WSGI environ entries to be merged into each request."""
        cipher = sock.cipher()
    +        if not cipher:
    +            return None
        ssl_environ = {
            "wsgi.url_scheme": "https",
            "HTTPS": "on",
            'SSL_PROTOCOL': cipher[1],
            'SSL_CIPHER': cipher[0]
    ##            SSL_VERSION_INTERFACE     string  The mod_ssl program version
    ##            SSL_VERSION_LIBRARY       string  The OpenSSL program version
            }
        return ssl_environ
    ```

OR as a patch
    ```
    cat << EOF > CherryPy-3.8.0-py2.7_out.patch
    71,75c71
    <         environ = self.get_environ(s)
    <         if environ:
    <             return s, environ
    <         else:
    <             return None, {}
    ---
    >         return s, self.get_environ(s)
    81,82d76
    <         if not cipher:
    <             return None
    
    
    EOF
    
    patch /usr/local/lib/python2.7/dist-packages/CherryPy-3.8.0-py2.7.egg/cherrypy/wsgiserver/ssl_builtin.py < CherryPy-3.8.0-py2.7_out.patch
    ```

### 6. Get, patch and install Openswan and KLIPS kernel module

**NOTE** We're using KLIPS in place of NETKEY as only KLIPS passed both tests:

* Multiple clients behind the same NAT can connect

* Multiple clients behind different NATs with the same local IP can connect


**NOTE2** We're compiling from source and backporting the following commit:

  http://git.openswan.org/cgi-bin/gitweb.cgi?p=openswan.git/.git;a=commit;h=b19ed5a79d0957fb50ac939559860d287cf762da

This is because it seems that versions that include this change have regressed to
not work well with multiple interfaces with names like ethx:y
We have to do this because we have multiple IP addresses per host.
The first patch described below allows more than 2 IP addresses per host.
The second patch described below allows more than 10 IP addresses per host.

**NOTE3** For Linode, follow these steps to use the kernel supplied by the linux distribution:

  http://library.linode.com/linode-platform/custom-instances/pv-grub-howto#sph_debian-6-squeeze

Also install linux-headers-2.6-xen-686

* Install necessary packages and configure:

    ```
    apt-get install -y bison flex libgmp3-dev
    # Get the latest Openswan package
    wget https://download.openswan.org/openswan/openswan-latest.tar.gz
    tar xzf openswan-latest.tar.gz
    cd cd openswan-2.6.45/  # or whatever the version downloaded is
    ```

--------------------------------------------------------------------------------

**NOTE - MULTIPLE IPs** 
The following code changes can be skipped if the server only has one 
host (i.e., only has one IP address).

* Edit `linux/include/openswan/ipsec_param.h`:
  Set *IPSEC_NUM_IF 20* or however number of interfaces are needed

  (See: http://git.openswan.org/cgi-bin/gitweb.cgi?p=openswan.git/.git;a=commitdiff;h=b19ed5a79d0957fb50ac939559860d287cf762da)

* Edit `lib/libipsecconf/virtif.c`:
  Change the function `valid_str()` to the following:
    ```
    static int valid_str(const char * const str, unsigned int * const pn, char ** const pphys)
    {
        char *pequal = NULL;
        char *pnum_start = NULL;
        char numeric[5] = {'\0'};
        char ch = '\0';
        unsigned int i = 0;
        if (!str) return 0;
        if (strlen(str)<8) return 0;

        /* Check if the string has an = sign */
        pequal = strchr(str,'=');
        if (!pequal) return 0;

        /* Where does the device number start ? */
        pnum_start = strstr(str,"ipsec");
        if (!pnum_start) {
            pnum_start = strstr(str,"mast");
            if (!pnum_start) return 0;
            else pnum_start += (sizeof("mast") - 1);
        }
        else pnum_start += (sizeof("ipsec") - 1);

        /* Is there a device number ? */
        if (pequal == pnum_start) return 0;

        /* Is there enough room to store the device number ? */
        if ((pequal - pnum_start) >= sizeof(numeric)) return 0;

        /* Copy only digit characters */
        while ( '=' != (ch = pnum_start[i]) ) {
            if (ch < '0' || ch > '9') return 0;
            numeric[i++] = ch;
        }

        if (pn) *pn = atoi(numeric);
        if (pphys) *pphys = pequal + 1;
        return 1;
    }
    ```

* Edit programs/_startklips/_startklips.in:
    ```
    Apply the following diff:
    -       $devprefix[0-9])        ;;
    +       $devprefix[0-9]*)       ;;
    ```

**END OF NOTE - MULTIPLE IPs**

--------------------------------------------------------------------------------

* Install Openswan
    ```
    make programs install
    make KERNELSRC=/lib/modules/`uname -r`/build module minstall
    ```

* Adjust ipsec Default-Start:
    ```
    sed -i 's/^# Default-Start:.*/# Default-Start:     2 3 4 5/' /etc/init.d/ipsec
    update-rc.d ipsec defaults
    /etc/init.d/ipsec restart
    ```

### 7. Configure GeoIP Lookups

```
cd $HOME
wget http://geolite.maxmind.com/download/geoip/api/c/GeoIP.tar.gz
tar xvf GeoIP.tar.gz
cd GeoIP-1.4.8
./configure && make && make install

cd ..
wget http://geolite.maxmind.com/download/geoip/api/python/GeoIP-Python-1.2.7.tar.gz
tar xzf GeoIP-Python-1.2.7.tar.gz
cd GeoIP-Python-1.2.7
python setup.py build && python setup.py install
```

### 8. Configure logging using rsyslog and logrotate

#### 1. Rsyslog

Direct all psiphonv logs to *psiphonv.log* and turn off all VPN logging 
(xl2tpd and pluto) that logs client IP addresses.

* Add to `/etc/rsyslog.d/50-default.conf`.  **IMPORTANT** Add new rules to the 
  top of the rules section
    ```
    (echo -n "
    ###############
    #### RULES ####
    ###############
    if \$programname == 'psiphonv' then /var/log/psiphonv.log
    if \$programname == 'psiphonv' then ~
    if \$programname == 'xl2tpd' then /dev/null
    if \$programname == 'xl2tpd' then ~
    if \$programname == 'pluto' then /dev/null
    if \$programname == 'pluto' then ~
    "; cat /etc/rsyslog.d/50-default.conf) > /etc/rsyslog.d/50-default.conf.new

    mv /etc/rsyslog.d/50-default.conf{.new,}
    ```

* Comment out *$ActionFileDefaultTemplate* in `/etc/rsyslog.conf`:
    ```
    sed -i 's/^\$ActionFileDefaultTemplate RSYSLOG_TraditionalFileFormat.*/#\$ActionFileDefaultTemplate RSYSLOG_TraditionalFileFormat\n/'  /etc/rsyslog.conf
    ```

* Restart the service
    ```
    /etc/init.d/rsyslog restart
    ```

#### 2. Logrotate

* Add to the weekly rotation (lower) section of /etc/logrotate.d/rsyslog

    ```
    sed -i 's/^\/var\/log\/messages.*/&\n\/var\/log\/psiphonv.log\n/' /etc/logrotate.d/rsyslog
    ```

--------------------------------------------------------------------------------

#### **End of host set up**
**NEXT STEPS** Set host as Psiphon Server.

--------------------------------------------------------------------------------
