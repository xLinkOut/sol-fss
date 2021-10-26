# SOL - File Storage Server 🗂
Final project for 'Operating Systems' curse @ UniPi. More info in `relazione/Relazione.pdf`.

## How to
### Compile
```bash
# Compile both client and server in build/
make
# Compile client and server separately
make client # build/client
make server # build/server
# Clean up object files (optional)
make clean
```

### Run
```bash
# Start the server, with a sample configuration file
./build/server config/config-example.txt
# Start the client, connect to the server and exit
./build/client -f ./fss.sk -p
```

### Test
```bash
# Basic
make test1
# Replacement Algorithm
make test2
# Stress test
make test3
# Clean up dummy files
make cleanall
```