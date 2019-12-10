#!/bin/bash -e

ROOT="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && cd tests && pwd )"

CONTAINER_NAME="nodeos"
NODEOS_IMAGE="eostudio/eos"
NODEOS_TAG="v2.0.0-rc2"

echo ""

rm -rf "$ROOT/blocks/" "$ROOT/state/"

echo "launching nodeos"
docker run -d -ti --name "${CONTAINER_NAME}" \
       -v "$ROOT:/app" \
       -v "$ROOT:/etc/nodeos" \
       -v "$ROOT:/data" \
       -p 127.0.0.1:8888:8888 -p 127.0.0.1:9876:9876 \
       ${NODEOS_IMAGE}:${NODEOS_TAG} \
       /usr/bin/nodeos --config-dir="/etc/nodeos" --data-dir="/data" --genesis-json="/etc/nodeos/genesis.json" --contracts-console 1> /dev/null
       

# Give time to Docker to boot up

echo "waiting for ready"
sleep 3

pushd $ROOT
eosc --debug -u http://localhost:8888 boot boot_seq.yaml 1> /tmp/${CONTAINER_NAME}-eos-bios-boot.log
echo "Boot completed"
echo ""
echo "Environment ready"
echo " API URL: http://localhost:9898"
echo " Info: eosc -u http://localhost:9898 get info"
echo " Logs: docker logs -f ${CONTAINER_NAME}"
echo ""

export EOSC_GLOBAL_INSECURE_VAULT_PASSPHRASE=secure

echo "Create testaccount3"
eosc -u http://localhost:8888 system newaccount testaccount1 testaccount3 --auth-key=EOS5MHPYyhjBjnQZejzZHqHewPWhGTfQWSVTWYEhDmJu4SXkzgweP --stake-cpu=10 --stake-net=10

echo "Post contract message"
eosc -u http://localhost:8888 tx create shine post '{"from":"testaccount1","to":"testaccount2","memo":"this is a test"}' -p testaccount1@active

echo "Vote"
eosc -u http://localhost:8888 tx create shine vote '{"voter":"testaccount3","post_id":"0"}' -p testaccount3@active


popd

