from fabric.api import *
import ec2
import config


env.key_filename = config.key_filename
env.user = config.instance_username

env.hosts = ['ec2-50-18-128-177.us-west-1.compute.amazonaws.com']


def install_packages():
    print 'Installing apt packages...'
    sudo('add-apt-repository -y ppa:chris-lea/node.js')
    sudo('apt-get update')
    sudo('apt-get -y install build-essential nodejs npm')
    print 'apt packages installed'

    print 'Installing npm packages...'
    put('package.json', 'package.json')
    # Uses the dependencies in package.json
    run('npm i')
    # Additional dependencies needed for this test
    run('npm i pty.js')
    print 'npm packages installed'


def upload_files():
    print 'Uploading files...'
    put('web-server-test.js', 'web-server-test.js')
    put('tunneled-request.js', 'tunneled-request.js')
    put('ssh-tunnel.js', 'ssh-tunnel.js')
    put(config.test_config_json_filename, 'test-conn-info.json')
    print 'Uploaded'


def web_server_test():
    print 'Running test...'
    run('node web-server-test.js %s' % config.test_propagation_channel_id)
    print 'Done'


def clear_results():
    run('rm *.csv')


def prepare():
    install_packages()


def go():
    clear_results()
    upload_files()
    web_server_test()
