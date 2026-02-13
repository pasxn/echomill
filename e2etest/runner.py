#!/usr/bin/env python3

import json
import os
import subprocess
import time
import urllib.request
import urllib.error
import socket
from pathlib import Path

class E2ETestRunner:
    def __init__(self, server_path, instruments_path):
        self.server_path = server_path
        self.instruments_path = instruments_path
        self.port = self._find_free_port()
        self.base_url = f"http://localhost:{self.port}"
        self.server_proc = None

    def _find_free_port(self):
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
            s.bind(('', 0))
            return s.getsockname()[1]

    def start_server(self):
        # need a new free port each time we start if we want to be safe
        self.port = self._find_free_port()
        self.base_url = f"http://localhost:{self.port}"
        
        print(f"--- Starting Echomill Server on port {self.port} ---")
        self.server_proc = subprocess.Popen(
            [self.server_path, str(self.port), self.instruments_path],
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            bufsize=1 # Line buffered
        )
        
        # Start a thread to stream output
        import threading
        def stream_output(pipe):
            for line in pipe:
                print(f"  [SERVER] {line.strip()}")
        
        self.stream_thread = threading.Thread(target=stream_output, args=(self.server_proc.stdout,))
        self.stream_thread.daemon = True
        self.stream_thread.start()

        # Wait for server to be ready
        retries = 20
        while retries > 0:
            try:
                with urllib.request.urlopen(f"{self.base_url}/status") as response:
                    if response.getcode() == 200:
                        return True
            except:
                time.sleep(0.1)
                retries -= 1
        return False

    def stop_server(self):
        if self.server_proc:
            self.server_proc.terminate()
            try:
                self.server_proc.wait(timeout=2)
            except subprocess.TimeoutExpired:
                self.server_proc.kill()
            self.server_proc = None

    def _subset_match(self, expected, actual):
        """Recursively check if 'expected' is a subset of 'actual'."""
        if isinstance(expected, dict):
            if not isinstance(actual, dict):
                return False
            for k, v in expected.items():
                if k not in actual or not self._subset_match(v, actual[k]):
                    return False
            return True
        elif isinstance(expected, list):
            if not isinstance(actual, list):
                return False
            # For simplicity, it is assumed that lists are ordered correctly or we match element-wise
            if len(expected) != len(actual):
                return False
            for e, a in zip(expected, actual):
                if not self._subset_match(e, a):
                    return False
            return True
        else:
            return str(expected) == str(actual)

    def run_scenario(self, scenario_path):
        with open(scenario_path, 'r') as f:
            scenario = json.load(f)

        name = scenario['meta'].get('name', scenario_path.name)
        print(f"\n> Running Scenario: {name}")
        
        # Start server for this scenario
        if not self.start_server():
            print("  FAILED: Could not start server.")
            return False

        try:
            for step in scenario['steps']:
                step_name = step.get('name', 'Unnamed step')
                action = step['action']
                parts = action.split(' ')
                method = parts[0]
                path = parts[1]
                url = f"{self.base_url}{path}"
                body = step.get('body')
                expect_status = step.get('expect_status', 200)
                expect_body = step.get('expect_body')

                print(f"  Step: {step_name} ({action})")
                
                req = urllib.request.Request(url, method=method)
                if body:
                    req.add_header('Content-Type', 'application/json')
                    data = json.dumps(body).encode('utf-8')
                else:
                    data = None

                try:
                    with urllib.request.urlopen(req, data=data) as response:
                        status = response.getcode()
                        resp_body = json.loads(response.read().decode('utf-8'))
                except urllib.error.HTTPError as e:
                    status = e.code
                    try:
                        resp_body = json.loads(e.read().decode('utf-8'))
                    except:
                        resp_body = {}
                except Exception as e:
                    print(f"    FAILED: Request error: {e}")
                    return False

                if status != expect_status:
                    print(f"    FAILED: Expected status {expect_status}, got {status}")
                    print(f"    Response: {json.dumps(resp_body)}")
                    return False

                if expect_body and not self._subset_match(expect_body, resp_body):
                    print(f"    FAILED: Body mismatch.")
                    print(f"      Expected (subset): {json.dumps(expect_body)}")
                    print(f"      Actual: {json.dumps(resp_body)}")
                    return False
        finally:
            self.stop_server()

        print(f"  PASSED: {name}")
        return True

def main():
    root = Path(__file__).parent.parent
    server_bin = root / "echomill/build/src/echomill_server"
    config = root / "config/instruments.json"
    scenarios_dir = Path(__file__).parent / "scenarios"

    if not server_bin.exists():
        print(f"Error: Server binary not found at {server_bin}")
        exit(1)

    runner = E2ETestRunner(str(server_bin), str(config))
    
    scenarios = sorted(scenarios_dir.glob("*.json"))
    if not scenarios:
        print("No scenarios found.")
        return

    success = True
    for s in scenarios:
        if not runner.run_scenario(s):
            success = False
            break

    if success:
        print("\nALL SCENARIOS PASSED!")
        exit(0)
    else:
        print("\nSCENARIO FAILED.")
        exit(1)

if __name__ == "__main__":
    main()
