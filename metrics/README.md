# CPS Metrics

## Overview

The usage of prometheus and grafana for monitoring is supported. <br/>
Each service exposes a set of metrics on the `/metrics` endpoint. <br/>
The metrics are collected by prometheus and visualized in a grafana dashboard.

## Port Configuration
The default ports for the metrics endpoints are as follows:
- **Generation Service**: `9102`
- **Processing Service**: `9103`