import boto
import boto.ec2
import config
import time
import collections


# Adapted from http://marthakelly.github.com/blog/2012/08/09/creating-an-ec2-instance-with-fabric-slash-boto/
def create_instances(count):
    '''
    Creates EC2 Instance
    '''
    print 'Started...'
    print '...Creating %d EC2 instances...' % count

    conn = boto.ec2.connect_to_region(config.ec2_region,
                                      aws_access_key_id=config.ec2_key,
                                      aws_secret_access_key=config.ec2_secret)

    image = conn.get_image(config.ec2_ami)

    reservation = image.run(count, count, key_name=config.ec2_key_pair,
                            security_groups=[config.ec2_security_group],
                            instance_type=config.ec2_instancetype)

    # Set name tags on all the instances
    conn.create_tags([i.id for i in reservation.instances],
                     {'Name': config.instance_name_tag})

    # Wait until they're all running
    while True:
        pendings = [i for i in reservation.instances if i.state == u'pending']
        if not pendings:
            break

        print '  Instances pending: %d' % len(pendings)
        time.sleep(10)

        [i.update() for i in pendings]

    print 'Instances states:'
    state_counter = collections.Counter([i.state for i in reservation.instances])
    for k in state_counter:
        print '  %s: %d' % (k, state_counter[k])

    print 'Done'

    # Return a list of Public DNS names
    return [i.public_dns_name for i in reservation.instances]


def terminate_instances(public_dns_names):
    '''
    Terminates all instances with the public DNS values in the list
    public_dns_names. Does not wait for the termination to complete.
    '''

    conn = boto.ec2.connect_to_region(config.ec2_region,
                                      aws_access_key_id=config.ec2_key,
                                      aws_secret_access_key=config.ec2_secret)

    print 'Getting existing instance list...'

    instances = []
    [instances.extend(reservation.instances)
        for reservation in conn.get_all_instances()]

    instances = [i for i in instances if i.public_dns_name in public_dns_names]

    print 'Found %d of %d matching instances. Terminating...' % (len(instances), len(public_dns_names))

    conn.terminate_instances([i.id for i in instances])

    print 'Done'
