# Tests

## Modifications to existing code.

The Windows client has been modified to spit out the file `test-conn-info.json`
after connecting. It contains the info needed to set up tunnels and make requests to a Psiphon server. This file is written to the Windows client directory after connecting.

## Running the tests from your Windows dev machine

### One-time Setup

1. Have Node.js installed.

2. PowerShell; `cd` to this directory.

3. Install need packages: `npm i`


### Run the tests

1. Run the debug build of the Windows client, with the debug command. Let it connect. 
  * You can then disconnect, if desired. The test code can create its own tunnel.

2. Run: `node web-server-test.js <test_propagation_channel_id>`


## Running the tests in the cloud

This is best run from a non-Windows machine. Fabric has a bug where it can't run
commands in parallel on Windows. This severely limits test behaviour when trying
to run from multiple instances.

You'll still need a `test-conn-info.json` file from Windows client, though.

### Set up your environment

* `pip install boto fabric`

### Set up the EC2 servers (which will be clients in the test).

1. `cd` into this directory.

2. Open a Python shell, then:
  
  ```python
  import ec2
  names = ec2.create_instances(<instance_count>, config.json)
  print names
  ```

  The public DNS names of the created instances will be printed out. Copy-paste
  them (with any needed changes) into the array at the bottom of `config.json`.

3. Prep the instances. At the command line, run `fab -P prepare` (the `-P` indicates that the job should be done in parallel on all the instances).

4. Run the tests. You can just run `fab -P go`, but I recommend doing some things
   in serial, so instead do: `fab clear_results -P upload_files -P web_server_test download_results` (note the absence of `-P` on the first and last command).

5. You will now have a local `results` directory with the results.
