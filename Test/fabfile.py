from fabric.api import *
import ec2
import config


env.key_filename = config.key_filename
env.user = config.instance_username

env.hosts = ['ec2-50-18-128-177.us-west-1.compute.amazonaws.com']


def uname():
    run('uname -a')


def install_packages():
    print 'Installing apt packages...'
    sudo('apt-get -y install build-essential nodejs npm')

    print 'Installing npm packages...'
    # Assumes the presence of a package.json file with dependencies
    run('npm i')

    # Additional dependencies needed for this test
    run('npm i pty.js')

    print 'Installed'


def upload_files():
    print 'Uploading files...'
    put('web-server-test.js', 'web-server-test.js')
    put('tunneled-request.js', 'tunneled-request.js')
    put('ssh-tunnel.js', 'ssh-tunnel.js')
    put('package.json', 'package.json')
    put(config.test_config_json_filename, 'test-conn-info.json')
    print 'Uploaded'


def web_server_test():
    print 'Running test...'
    run('node web-server-test.js %s' % config.test_propagation_channel_id)
    print 'Done'


def go():
    upload_files()
    install_packages()
    web_server_test()
