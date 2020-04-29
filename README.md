# The Shine smart contract for EOSIO

## Build

Go modify the `EOS_SYMBOL` in `asset.hpp` before compiling.

```
cd contract
./build.sh
```

## Deploy

```

dfuseeos start

...

export EOSC_GLOBAL_API_URL=http://localhost:8888
export EOSC_GLOBAL_VAULT_FILE=$HOME/dfuse/dfuse-eosio/tools/eosc-vault.json

pushd ../dfuseeos/tools
eosc boot bootseq.yaml
popd

eosc system setcontract build/shine.wasm build/shine.abi

```
