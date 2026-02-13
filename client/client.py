#!/usr/bin/env python3

import argparse
import requests
import json
try:
    import readline
except ImportError:
    readline = None
import shlex
from typing import Dict, Any, List

class EchoMillClient:
    def __init__(self, host: str, port: int):
        self.base_url = f"http://{host}:{port}"
        self.next_order_id = 1000

    def post_order(self, symbol: str, side: int, price: int, qty: int, order_id: int = None, order_type: int = 1):
        if order_id is None:
            order_id = self.next_order_id
            self.next_order_id += 1
            
        payload = {
            "symbol": symbol,
            "side": side,
            "price": price,
            "qty": qty,
            "id": order_id,
            "type": order_type
        }
        try:
            response = requests.post(f"{self.base_url}/orders", json=payload)
            self._handle_response(response)
        except requests.exceptions.ConnectionError:
            print(f"Error: Could not connect to server at {self.base_url}")

    def cancel_order(self, order_id: int):
        payload = {"id": order_id}
        try:
            response = requests.delete(f"{self.base_url}/orders", json=payload)
            self._handle_response(response)
        except requests.exceptions.ConnectionError:
            print(f"Error: Could not connect to server at {self.base_url}")

    def get_depth(self, symbol: str, levels: int = 5):
        try:
            params = {"symbol": symbol, "levels": levels}
            response = requests.get(f"{self.base_url}/depth", params=params)
            if response.status_code == 200:
                data = response.json()
                self._print_depth(symbol, data)
            else:
                self._handle_response(response)
        except requests.exceptions.ConnectionError:
            print(f"Error: Could not connect to server at {self.base_url}")

    def get_status(self):
        try:
            response = requests.get(f"{self.base_url}/status")
            self._handle_response(response)
        except requests.exceptions.ConnectionError:
            print(f"Error: Could not connect to server at {self.base_url}")

    def get_trades(self):
        try:
            response = requests.get(f"{self.base_url}/trades")
            self._handle_response(response)
        except requests.exceptions.ConnectionError:
            print(f"Error: Could not connect to server at {self.base_url}")

    def _handle_response(self, response: requests.Response):
        try:
            data = response.json()
            if "error" in data:
                print(f"\033[91mERROR: {data['error']}\033[0m")
            else:
                print(json.dumps(data, indent=2))
        except ValueError:
            print(f"HTTP {response.status_code}: {response.text}")

    def _print_depth(self, symbol: str, data: Dict[str, Any]):
        print(f"\n--- Order Book: {symbol} ---")
        
        asks = data.get("asks", [])
        bids = data.get("bids", [])
        
        print(f"{'Side':<10} {'Price':<10} {'Qty':<10} {'Count':<6}")
        print("-" * 40)
        
        # Asks in descending price order (to put best ask at bottom of red section)
        for ask in reversed(asks):
            print(f"\033[91m{'ASK':<10} {ask['price']:<10} {ask['qty']:<10} {ask.get('count', '-'):<6}\033[0m")
            
        if not asks and not bids:
            print(f"{'EMPTY':^40}")
        elif not asks:
            print(f"{'--- no asks ---':^40}")
            
        print("-" * 40)
        
        if not bids:
            print(f"{'--- no bids ---':^40}")
        for bid in bids:
            print(f"\033[92m{'BID':<10} {bid['price']:<10} {bid['qty']:<10} {bid.get('count', '-'):<6}\033[0m")
            
        print("-" * 40)

def run_interactive(client: EchoMillClient):
    print("EchoMill Interactive Client")
    print("Type 'help' for commands, 'exit' or Ctrl-D to quit.")
    
    while True:
        try:
            line = input("\033[1mechomill>\033[0m ")
            if not line.strip():
                continue
                
            parts = shlex.split(line)
            cmd = parts[0].lower()
            
            if cmd == "exit" or cmd == "quit":
                break
            elif cmd == "help":
                print("Commands:")
                print("  buy <symbol> <qty> <price> [--id ID] [--market]")
                print("  sell <symbol> <qty> <price> [--id ID] [--market]")
                print("  cancel <id>")
                print("  depth <symbol> [levels]")
                print("  status")
                print("  trades")
                print("  exit")
            elif cmd in ["buy", "sell"]:
                if len(parts) < 4:
                    print("Usage: buy/sell <symbol> <qty> <price>")
                    continue
                symbol = parts[1]
                qty = int(parts[2])
                price = int(parts[3])
                side = 1 if cmd == "buy" else -1
                client.post_order(symbol, side, price, qty)
            elif cmd == "cancel":
                if len(parts) < 2:
                    print("Usage: cancel <id>")
                    continue
                client.cancel_order(int(parts[1]))
            elif cmd == "depth":
                if len(parts) < 2:
                    print("Usage: depth <symbol> [levels]")
                    continue
                levels = int(parts[2]) if len(parts) > 2 else 5
                client.get_depth(parts[1], levels)
            elif cmd == "status":
                client.get_status()
            elif cmd == "trades":
                client.get_trades()
            else:
                print(f"Unknown command: {cmd}")
                
        except EOFError:
            print()
            break
        except Exception as e:
            print(f"Error: {e}")

def main():
    parser = argparse.ArgumentParser(description="EchoMill CLI Client")
    parser.add_argument("--host", default="localhost", help="Server host")
    parser.add_argument("--port", type=int, default=8080, help="Server port")
    
    subparsers = parser.add_subparsers(dest="command")
    
    # Buy/Sell
    for cmd in ["buy", "sell"]:
        p = subparsers.add_parser(cmd)
        p.add_argument("symbol", help="Trading symbol")
        p.add_argument("qty", type=int, help="Quantity")
        p.add_argument("price", type=int, help="Price")
        p.add_argument("--id", type=int, help="Order ID")
        p.add_argument("--market", action="store_true", help="Market order")

    # Cancel
    p = subparsers.add_parser("cancel")
    p.add_argument("id", type=int, help="Order ID")

    # Depth
    p = subparsers.add_parser("depth")
    p.add_argument("symbol", help="Trading symbol")
    p.add_argument("--levels", type=int, default=5, help="Levels")

    subparsers.add_parser("status")
    subparsers.add_parser("trades")

    args = parser.parse_args()
    client = EchoMillClient(args.host, args.port)

    if args.command is None:
        run_interactive(client)
    elif args.command in ["buy", "sell"]:
        side = 1 if args.command == "buy" else -1
        order_type = 2 if args.market else 1
        client.post_order(args.symbol, side, args.price, args.qty, args.id, order_type)
    elif args.command == "cancel":
        client.cancel_order(args.id)
    elif args.command == "depth":
        client.get_depth(args.symbol, args.levels)
    elif args.command == "status":
        client.get_status()
    elif args.command == "trades":
        client.get_trades()

if __name__ == "__main__":
    main()
