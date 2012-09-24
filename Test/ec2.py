import boto
import boto.ec2
import time
import collections  # collections.Counter requires Python 2.7+
import json


def _read_config(conf_file):
    with open(conf_file) as conf_fp:
        config = json.load(conf_fp)
    return config


# Adapted from http://marthakelly.github.com/blog/2012/08/09/creating-an-ec2-instance-with-fabric-slash-boto/
def create_instances(count, conf_file):
    '''
    Creates EC2 instances. `count` is the number of instances to create.
    Info about the new instances comes from the `config` file.
    Monitors creation until completion.
    '''

    config = _read_config(conf_file)

    print 'Connecting to region %s...' % config['ec2_region']

    conn = boto.ec2.connect_to_region(config['ec2_region'],
                                      aws_access_key_id=config['ec2_key'],
                                      aws_secret_access_key=config['ec2_secret'])

    print '...Creating %d EC2 instances...' % count

    image = conn.get_image(config['ec2_ami'])

    reservation = image.run(count, count, key_name=config['ec2_key_pair'],
                            security_groups=[config['ec2_security_group']],
                            instance_type=config['ec2_instancetype'])

    # Set name tags on all the instances
    conn.create_tags([i.id for i in reservation.instances],
                     {'Name': config['instance_name_tag']})

    # Wait until they're all running
    while True:
        pendings = [i for i in reservation.instances if i.state == u'pending']
        if not pendings:
            break

        print '  Instances pending: %d...' % len(pendings)
        time.sleep(10)

        [i.update() for i in pendings]

    print 'Instances states:'
    state_counter = collections.Counter([i.state for i in reservation.instances])
    for k in state_counter:
        print '  %s: %d' % (k, state_counter[k])

    print 'Done'

    # Return a list of Public DNS names
    return [i.public_dns_name for i in reservation.instances]


def terminate_instances(public_dns_names, conf_file):
    '''
    Terminates all instances with the public DNS values in the list
    `public_dns_names`. Does not wait for the termination to complete.
    '''

    config = _read_config(conf_file)

    print 'Connecting to region %s...' % config['ec2_region']

    conn = boto.ec2.connect_to_region(config['ec2_region'],
                                      aws_access_key_id=config['ec2_key'],
                                      aws_secret_access_key=config['ec2_secret'])

    print 'Getting existing instance list...'

    instances = []
    [instances.extend(reservation.instances)
        for reservation in conn.get_all_instances()]

    instances = [i for i in instances if i.public_dns_name in public_dns_names]

    print 'Found %d of %d matching instances. Terminating...' % (len(instances), len(public_dns_names))

    conn.terminate_instances([i.id for i in instances])

    print 'Done'


def list_instances(conf_file):
    '''
    Lists running instances that have the currently configured `Name` tag.
    Displays info about them and returns an array of their public DNS names.
    '''

    config = _read_config(conf_file)

    print 'Connecting to region %s...' % config['ec2_region']

    conn = boto.ec2.connect_to_region(config['ec2_region'],
                                      aws_access_key_id=config['ec2_key'],
                                      aws_secret_access_key=config['ec2_secret'])

    print 'Getting existing instance list...'

    instances = []
    [instances.extend(reservation.instances)
        for reservation
        in conn.get_all_instances(filters={'tag:Name': config['instance_name_tag'],
                                           'instance-state-name': 'running'})]

    result = [i.public_dns_name for i in instances if i.public_dns_name]

    # We could print out more info, but for now...
    print result

    return result
