# CPS Deployment
Allows you to deploy the CPS on any host using Ansible, with the ability to customize the configuration for each host (e.g., RSU, PIXKIT, HOLOLENS, BIKE, etc.).

### Run the following command to execute the Ansible playbook:

```bash
ansible-playbook deploy.yml -i inventory/hosts.yml
```

To change the target host, edit the `inventory/hosts.yml` file, which already contains various examples.

### Variable Configuration

The full list of configurable variables can be found under **cps_configs** in the `defaults/main.yml` file. 
To override any of these variables, you can add a **cps_overrides** section in any host of the `inventory/hosts.yml` file.