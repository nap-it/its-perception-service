
## [`config.ini`](https://code.nap.av.it.pt/mobility-networks/cps-v2/-/blob/main/sensor-adapters/autoware-adapter/config.ini) configuration

The contents of the `config.ini`are as follows:

```ini
[autoware-adapter]
debug=false
domain_id=0
```

| .ini file key                 | Default                       | Notes |
| -------------                 |-------------                      |-------------|
| domain_id                 | 0             | **Change this** depending on the VPI domain id |
| debug                         | false     | Debug flag to extend the logs|