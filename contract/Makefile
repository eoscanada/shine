BLOCKCHAIN_HOST := "localhost"
BLOCKCHAIN_PORT := "8888"
CONTRACT_ACCOUNT := "shine.v1"
WALLET_HOST := "localhost"
WALLET_PORT := "6666"

HEADERS = asset.hpp shine.hpp table.hpp

default: build

build: shine.wast

shine.wast: shine.cpp $(HEADERS)
	eosiocpp -o shine.wast shine.cpp shine.abi

install: build
	cleos \
	  --host ${BLOCKCHAIN_HOST} \
		--port ${BLOCKCHAIN_PORT} \
		--wallet-host ${WALLET_HOST} \
		--wallet-port ${WALLET_PORT} \
		set contract ${CONTRACT_ACCOUNT} `pwd` \
		shine.wast shine.abi

clean:
	-@rm -f shine.wast 2>/dev/null || true