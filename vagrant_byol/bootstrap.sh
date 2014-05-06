#!/usr/bin/env bash

apt-get update
apt-get install -y git-core
apt-get install -y emacs
apt-get install -y libedit-dev


git config user.name --global "Aric Huang"
git config user.email --global "arichuang@gmail.com"
git config credential.helper store

rm -rf /var/www
ln -fs /vagrant /var/www