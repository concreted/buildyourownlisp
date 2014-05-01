#!/usr/bin/env bash

apt-get update
apt-get install -y git-core
apt-get install -y emacs
apt-get install -y libedit-dev

rm -rf /var/www
ln -fs /vagrant /var/www