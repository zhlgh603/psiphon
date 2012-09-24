from fabric.api import *
import json


with open('dist-test-config.json') as conf_fp:
    config = json.load(conf_fp)

env.key_filename = config['key_filename']
env.user = config['instance_username']

env.hosts = config['test_hosts']


def _read_config(conf_file):
    with open(conf_file) as conf_fp:
        config = json.load(conf_fp)
    return config


def install_packages():
    print 'Installing apt packages...'
    sudo('add-apt-repository -y ppa:chris-lea/node.js')
    sudo('apt-get -qq update')
    sudo('apt-get -y -qq install build-essential nodejs npm polipo')
    sudo('service polipo stop')
    print 'apt packages installed'

    print 'Installing npm packages...'
    put('package.json', 'package.json')
    # Uses the dependencies in package.json
    run('npm i &> /dev/null')
    # Additional dependencies needed for this test
    run('npm i pty.js &> /dev/null')
    print 'npm packages installed'


def upload_files():
    print 'Uploading files...'
    put('web-server-test.js', 'web-server-test.js')
    put('tunneled-request.js', 'tunneled-request.js')
    put('ssh-tunnel.js', 'ssh-tunnel.js')
    put(config['test_config_json_filename'], 'test-conn-info.json')
    print 'Uploaded'


def download_results():
    print 'Downloading results...'
    local('mkdir -p results')
    get('*.csv', 'results/%(host)s-%(basename)s')
    print 'Results downloaded'


def web_server_test():
    print 'Running test...'
    run('node web-server-test.js %s' % config['test_propagation_channel_id'])
    print 'Done'


def clear_results():
    local('rm -rf results')
    run('rm -f *.csv')


def prepare():
    install_packages()


def go():
    clear_results()
    upload_files()
    web_server_test()

    download_results()
