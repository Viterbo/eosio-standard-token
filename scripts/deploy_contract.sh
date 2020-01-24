#!/bin/bash

force=false
NET=

if [ "$1" == "prod" ]; then
   NET='--url https://telos.eos.barcelona'
fi

if [ "$1" == "test" ]; then
   NET='--url https://testnet.telos.caleos.io'
fi

simpletoken_HOME=../
echo "-------- simpletoken ---------"
cd $simpletoken_HOME
eosio-cpp -o simpletoken.wasm simpletoken.cpp --abigen
echo "$ cleos $NET set contract simpletoken $PWD -p simpletoken@active"
cleos $NET set contract simpletoken $PWD -p simpletoken@active




