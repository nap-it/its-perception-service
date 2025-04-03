import paramiko
import logging

# Custom formatter to colorize INFO-level messages green.
class ColorFormatter(logging.Formatter):
    GREEN = "\033[32m"  # ANSI code for green
    ORANGE = "\033[38;5;208m"  # ANSI code for orange
    RESET = "\033[0m"   # ANSI code to reset color

    def format(self, record):
        message = super().format(record)
        if record.levelno == logging.WARNING:
            return f"{self.ORANGE}{message}{self.RESET}"
        return message

# Setup logging with our custom formatter.
handler = logging.StreamHandler()
formatter = ColorFormatter("%(asctime)s - %(levelname)s - %(message)s")
handler.setFormatter(formatter)

logger = logging.getLogger()
logger.setLevel(logging.INFO)
logger.handlers = []  # Clear existing handlers if any
logger.addHandler(handler)

# Format: host_ip: (name, username, password, service_type)
hosts = {
    "10.0.25.3": ("p3", "nap", "openlab", "radar"),
    "atcll-p6-jetson.nap.av.it.pt": ("p6", "jetson", "openlab", "camera"),
    "10.0.22.204": ("p22", "nap", "openlab", "camera"),
    "atcll-p25-jetson.nap.av.it.pt": ("p25", "jetson", "openlab", "camera"),
    "atcll-p30-jetson.nap.av.it.pt": ("p30", "jetson", "openlab", "both"),
    "atcll-p33-jetson.nap.av.it.pt": ("p33", "jetson", "openlab", "both"),
    "atcll-p35-jetson.nap.av.it.pt": ("p35", "jetson", "openlab", "both"),
}

def run_command(client, command):
    """
    Execute a command on the SSH client and log its execution result.
    """
    logging.info(f"Running command: {command}")
    stdin, stdout, stderr = client.exec_command(command)
    # Wait for the command to complete and fetch the exit status.
    exit_status = stdout.channel.recv_exit_status()
    output = stdout.read().decode().strip()
    error = stderr.read().decode().strip()
    if exit_status == 0:
        logging.info(f"Command succeeded: {command}")
        if output:
            logging.info(f"Output: {output}")
    else:
        logging.error(f"Command failed: {command}")
        logging.error(f"Exit Status: {exit_status}")
        if error:
            logging.error(f"Error: {error}")

def main():
    for host, (name, username, password, service_type) in hosts.items():
        logging.warning(f"--- Processing host {name} ---")
        logging.warning(f"Connecting to host {host} with username: {username}")
        client = paramiko.SSHClient()
        client.set_missing_host_key_policy(paramiko.AutoAddPolicy())
        try:
            client.connect(hostname=host, username=username, password=password)
            logging.warning(f"Successfully connected to {host}")

            # Step 1: Navigate to the /services/cps directory.
            run_command(client, "cd /services/cps")

            # Step 2: Pull the generation image.
            logging.warning("Pulling generation image...")
            run_command(client, "docker pull code.nap.av.it.pt:5050/mobility-networks/cps-v2/generation:dcc")

            # Step 3: Pull the processing image.
            logging.warning("Pulling processing image...")
            run_command(client, "docker pull code.nap.av.it.pt:5050/mobility-networks/cps-v2/processing:dcc")

            # Step 4: Conditionally pull the radar adapter image if required.
            if service_type.lower() in ["radar", "both"]:
                logging.warning("Pulling radar adapter image...")
                run_command(client, "docker pull code.nap.av.it.pt:5050/mobility-networks/cps-v2/radar-adapter:dcc")

            # Step 5: Conditionally pull the camera adapter image if required.
            if service_type.lower() in ["camera", "both"]:
                logging.warning("Pulling camera adapter image...")
                run_command(client, "docker pull code.nap.av.it.pt:5050/mobility-networks/cps-v2/camera-adapter:dcc")

            # Step 6: Restart docker compose (bring down and up the services).
            logging.warning("Restarting docker compose...")
            run_command(client, "cd /services/cps && docker compose down && docker compose up -d")

            # Step 7: Check the status of the services.
            logging.warning("Checking status of services...")
            run_command(client, "cd /services/cps && docker compose ps")
            
            logging.warning(f"--- Finished processing host {name} ---\n")
        except Exception as e:
            logging.error(f"An error occurred with host {name}: {e}")
        finally:
            client.close()
            logging.warning(f"Connection to host {name} closed.\n")

if __name__ == '__main__':
    main()