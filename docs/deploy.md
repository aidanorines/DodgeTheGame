# Deploying the online server

`dodge_server` is a headless program for a Linux VPS. It keeps parties only in memory, so restarting it ends every party.

## Build

On Ubuntu 24.04:

```bash
sudo apt-get install -y build-essential cmake git
git clone https://github.com/aidanorines/DodgeTheGame.git
cd DodgeTheGame
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DDODGE_SERVER_ONLY=ON
cmake --build build --target dodge_server -j
```

`DODGE_SERVER_ONLY` skips raylib, raygui, the client and the tests, so no graphics packages are needed.

## Open the port

The game uses UDP 7777. Open it in the VPS provider's firewall too, if it has one.

```bash
sudo ufw allow 7777/udp
```

## Run as a service

Create `/etc/systemd/system/dodge.service`, with the path changed to your build:

```ini
[Unit]
Description=Dodge game server
After=network.target

[Service]
ExecStart=/home/dodge/DodgeTheGame/build/dodge_server --port 7777
Restart=on-failure
DynamicUser=yes

[Install]
WantedBy=multi-user.target
```

```bash
sudo systemctl daemon-reload
sudo systemctl enable --now dodge
```

## Point clients at it

Copy `dodge.cfg.example` to `dodge.cfg` in the folder the game is started from, and set `host=` to the VPS address.

## Updating

Clients and the server must have the same `kProtocolVersion`. Bump it in `shared/constants.h` when messages change. Old clients then see "Game version mismatch".
